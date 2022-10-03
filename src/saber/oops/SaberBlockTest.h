/*
 * (C) Copyright 2022 UCAR
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/range/adaptors.hpp>

#include "oops/base/Increment.h"
#include "oops/base/State.h"
#include "oops/base/Variables.h"
#include "oops/mpi/mpi.h"
#include "oops/runs/Application.h"
#include "oops/util/Logger.h"
#include "oops/util/parameters/Parameter.h"
#include "oops/util/parameters/RequiredParameter.h"
#include "oops/util/Random.h"

#include "saber/oops/ReadInputFields.h"
#include "saber/oops/SaberCentralBlockBase.h"
#include "saber/oops/SaberCentralBlockParametersBase.h"
#include "saber/oops/SaberOuterBlockBase.h"
#include "saber/oops/SaberOuterBlockParametersBase.h"

namespace eckit {
  class Configuration;
}

namespace oops {
  class Variables;
}

namespace saber {

// -----------------------------------------------------------------------------

template <typename MODEL> class SaberBlockTestParameters
  : public oops::ApplicationParameters {
  OOPS_CONCRETE_PARAMETERS(SaberBlockTestParameters, oops::ApplicationParameters)

 public:
  typedef typename oops::Geometry<MODEL>::Parameters_ GeometryParameters_;
  typedef typename oops::State<MODEL>::Parameters_    StateParameters_;

  /// Geometry parameters
  oops::RequiredParameter<GeometryParameters_> geometry{"geometry", this};

  /// Tested variables
  oops::RequiredParameter<oops::Variables> variables{"variables", this};

  /// Background parameters
  oops::RequiredParameter<StateParameters_> background{"background", this};

  /// SABER blocks
  oops::OptionalParameter<SaberCentralBlockParametersWrapper>
    saberCentralBlock{"saber central block", this};
  oops::OptionalParameter<std::vector<SaberOuterBlockParametersWrapper>>
    saberOuterBlocks{"saber outer blocks", this};

  /// Adjoint test tolerance
  oops::Parameter<double> adjointTolerance{"adjoint test tolerance", 1.0e-12, this};

  /// Inverse test tolerance
  oops::Parameter<double> inverseTolerance{"inverse test tolerance", 1.0e-12, this};
};

// -----------------------------------------------------------------------------

template <typename MODEL> class SaberBlockTest : public oops::Application {
  typedef oops::Geometry<MODEL>                                Geometry_;
  typedef oops::Increment<MODEL>                               Increment_;
  typedef typename boost::ptr_vector<SaberOuterBlockBase>      SaberOuterBlockVec_;
  typedef oops::State<MODEL>                                   State_;

 public:
  static const std::string classname() {return "saber::SaberBlockTest";}
  explicit SaberBlockTest(const eckit::mpi::Comm & comm = oops::mpi::world())
    : Application(comm) {}

  virtual ~SaberBlockTest() {}

  int execute(const eckit::Configuration & fullConfig, bool validate) const override {
    util::Timer timer(classname(), "execute");

    // Deserialize parameters
    SaberBlockTestParameters<MODEL> params;
    if (validate) params.validate(fullConfig);
    params.deserialize(fullConfig);

    // Setup geometry
    const Geometry_ geom(params.geometry, this->getComm());

    // Setup variables
    const oops::Variables incVars(params.variables);

    // Setup background state
    const State_ xx(geom, params.background);

    // SABER blocks
    std::unique_ptr<SaberCentralBlockBase> saberCentralBlock_;
    SaberOuterBlockVec_ saberOuterBlocks_;

    // Local copy of background and first guess
    State_ xbLocal(xx);
    State_ fgLocal(xx);

    // Initial output geometry and variables
    std::vector<std::reference_wrapper<const oops::GeometryData>> outputGeometryData_;
    outputGeometryData_.push_back(geom.generic());
    oops::Variables outputVars(incVars);

    // Build outer blocks successively
    const boost::optional<std::vector<SaberOuterBlockParametersWrapper>> &saberOuterBlocks =
      params.saberOuterBlocks.value();
    if (saberOuterBlocks != boost::none) {
      // Loop in reverse order
      for (const SaberOuterBlockParametersWrapper & saberOuterBlockParamWrapper :
        boost::adaptors::reverse(*saberOuterBlocks)) {
        // Get outer block parameters
        const SaberOuterBlockParametersBase & saberOuterBlockParams =
          saberOuterBlockParamWrapper.saberOuterBlockParameters;

        // Get active variables
        oops::Variables activeVars =
          saberOuterBlockParams.activeVars.value().get_value_or(outputVars);

        // Read input fields (on model increment geometry)
        std::vector<atlas::FieldSet> fsetVec = readInputFields(
          geom,
          activeVars,
          xx.validTime(),
          saberOuterBlockParams.inputFields.value());

        // Create outer block
        oops::Log::info() << "Creating outer block: "
                          << saberOuterBlockParams.saberBlockName.value() << std::endl;
        saberOuterBlocks_.push_back(SaberOuterBlockFactory::create(
                                    outputGeometryData_.back().get(),
                                    geom.variableSizes(activeVars),
                                    outputVars,
                                    saberOuterBlockParams,
                                    xbLocal.fieldSet(),
                                    fgLocal.fieldSet(),
                                    fsetVec));

        // Apply calibration inverse on xb and fg
        saberOuterBlocks_.back().calibrationInverseMultiply(xbLocal.fieldSet());
        saberOuterBlocks_.back().calibrationInverseMultiply(fgLocal.fieldSet());

        // Access input geometry and variables
        const oops::GeometryData & inputGeometryData = saberOuterBlocks_.back().inputGeometryData();
        const oops::Variables inputVars = saberOuterBlocks_.back().inputVars();

        // Check that active variables are present in either input or output variables, or both
        for (const auto & var : activeVars.variables()) {
          ASSERT(inputVars.has(var) || outputVars.has(var));
        }

        // Adjoint test

        // Variables sizes
        std::vector<size_t> inputVariableSizes = geom.variableSizes(inputVars);

        // Create random input FieldSet
        atlas::FieldSet inputFset = createRandomFieldSet(inputGeometryData.functionSpace(),
                                                         inputVariableSizes,
                                                         inputVars);

        // Copy input FieldSet
        atlas::FieldSet inputFsetSave = copyFieldSet(inputFset);

        // Variables sizes
        std::vector<size_t> outputVariableSizes = geom.variableSizes(outputVars);

        // Create random output FieldSet
        atlas::FieldSet outputFset =
          createRandomFieldSet(outputGeometryData_.back().get().functionSpace(),
                               outputVariableSizes,
                               outputVars);

        // Copy output FieldSet
        atlas::FieldSet outputFsetSave = copyFieldSet(outputFset);

        // Apply forward and adjoint multiplication
        saberOuterBlocks_.back().multiply(inputFset);
        saberOuterBlocks_.back().multiplyAD(outputFset);

        // Compute adjoint test
        const double dp1 = dot_product(inputFset, outputFsetSave, geom.getComm());
        const double dp2 = dot_product(outputFset, inputFsetSave, geom.getComm());
        oops::Log::info() << "Adjoint test for outer block "
                          << saberOuterBlockParams.saberBlockName.value()
                          << ": y^t (Ax) = " << dp1 << ": x^t (A^t y) = " << dp2 << std::endl;
        ASSERT(abs(dp1) > 0.0);
        ASSERT(abs(dp2) > 0.0);
        oops::Log::test() << "Adjoint test for outer block "
                          << saberOuterBlockParams.saberBlockName.value();
        if (0.5*abs(dp1-dp2)/(dp1+dp2) < params.adjointTolerance.value()) {
          oops::Log::test() << " passed" << std::endl;
        } else {
          oops::Log::test() << " failed" << std::endl;
          ABORT("Adjoint test failure");
        }

        // Update output geometry and variables for the next block
        outputGeometryData_.push_back(inputGeometryData);
        outputVars = inputVars;
      }
    }

    const boost::optional<SaberCentralBlockParametersWrapper> &saberCentralBlock =
      params.saberCentralBlock.value();
    if (saberCentralBlock != boost::none) {
      // Get central block parameters
      const SaberCentralBlockParametersBase & saberCentralBlockParams =
        saberCentralBlock->saberCentralBlockParameters;

      // Define input/output variables
      oops::Variables inoutVars = outputVars;

      // Get active variables
      oops::Variables activeVars =
        saberCentralBlockParams.activeVars.value().get_value_or(outputVars);

      // Read input fields (on model increment geometry)
      std::vector<atlas::FieldSet> fsetVec = readInputFields(
        geom,
        activeVars,
        xx.validTime(),
        saberCentralBlockParams.inputFields.value());

      // Create central block
      saberCentralBlock_.reset(SaberCentralBlockFactory::create(
                               outputGeometryData_.back().get(),
                               geom.variableSizes(activeVars),
                               inoutVars,
                               saberCentralBlockParams,
                               xbLocal.fieldSet(),
                               fgLocal.fieldSet(),
                               fsetVec));

      // Check that active variables are present in input/output variables
      for (const auto & var : activeVars.variables()) {
        ASSERT(inoutVars.has(var));
      }

      // Adjoint test

      // Variables sizes
      std::vector<size_t> inoutVariableSizes = geom.variableSizes(inoutVars);

      // Create random input FieldSet
      atlas::FieldSet inputFset =
        createRandomFieldSet(outputGeometryData_.back().get().functionSpace(),
                             inoutVariableSizes,
                             inoutVars);

      // Copy input FieldSet
      atlas::FieldSet inputFsetSave = copyFieldSet(inputFset);

      // Create random output FieldSet
      atlas::FieldSet outputFset =
        createRandomFieldSet(outputGeometryData_.back().get().functionSpace(),
                             inoutVariableSizes,
                             inoutVars);

      // Copy output FieldSet
      atlas::FieldSet outputFsetSave = copyFieldSet(outputFset);

      // Apply forward multiplication only (self-adjointness test)
      saberCentralBlock_->multiply(inputFset);
      saberCentralBlock_->multiply(outputFset);

      // Compute adjoint test
      const double dp1 = dot_product(inputFset, outputFsetSave, geom.getComm());
      const double dp2 = dot_product(outputFset, inputFsetSave, geom.getComm());
      oops::Log::info() << "Adjoint test for central block "
                        << saberCentralBlockParams.saberBlockName.value()
                        << ": y^t (Ax) = " << dp1 << ": x^t (Ay) = " << dp2 << std::endl;
      ASSERT(abs(dp1) > 0.0);
      ASSERT(abs(dp2) > 0.0);
      oops::Log::test() << "Adjoint test for central block "
                        << saberCentralBlockParams.saberBlockName.value();
      if (0.5*abs(dp1-dp2)/(dp1+dp2) < params.adjointTolerance.value()) {
        oops::Log::test() << " passed" << std::endl;
      } else {
        oops::Log::test() << " failed" << std::endl;
        ABORT("Adjoint test failure");
      }
    }

    return 0;
  }

 private:
  std::string appname() const override {
    return "saber::SaberBlockTest<" + MODEL::name() + ">";
  }

  // -----------------------------------------------------------------------------
  // TODO(Benjamin): should be moved in OOPS?
  atlas::FieldSet createRandomFieldSet(const atlas::FunctionSpace & functionSpace,
                                       const std::vector<size_t> & variableSizes,
                                       const oops::Variables & vars) const {
    // Get ghost points
    atlas::Field ghost;
    if (functionSpace.type() == "Spectral") {
      ghost = functionSpace.createField<int>(atlas::option::name("ghost"));
      auto ghostView = atlas::array::make_view<int, 1>(ghost);
      for (atlas::idx_t jnode = 0; jnode < ghost.shape(0); ++jnode) {
        ghostView(jnode) = 0;
      }
    } else {
      ghost = functionSpace.ghost();
    }
    auto ghostView = atlas::array::make_view<int, 1>(ghost);

    // Create FieldSet
    atlas::FieldSet fset;

    for (size_t jvar = 0; jvar < vars.size(); ++jvar) {
      // Create field
      atlas::Field field = functionSpace.createField<double>(
        atlas::option::name(vars.variables()[jvar]) | atlas::option::levels(variableSizes[jvar]));

      // Get field owned size
      size_t n = 0;
      if (field.rank() == 2) {
        if (functionSpace.type() == "Spectral") {
          atlas::functionspace::Spectral fs(functionSpace);
          const atlas::idx_t N = fs.truncation();
          const auto zonal_wavenumbers = fs.zonal_wavenumbers();
          const atlas::idx_t nb_zonal_wavenumbers = zonal_wavenumbers.size();
          for (int jm=0; jm < nb_zonal_wavenumbers; ++jm) {
            const atlas::idx_t m1 = zonal_wavenumbers(jm);
            for (std::size_t n1 = m1; n1 <= static_cast<std::size_t>(N); ++n1) {
              if (m1 == 0) {
                n += field.shape(1);
              } else {
                n += 2*field.shape(1);
              }
            }
          }
        } else {
          for (atlas::idx_t jnode = 0; jnode < field.shape(0); ++jnode) {
            if (ghostView(jnode) == 0) n += field.shape(1);
          }
        }
      }

      // Generate random vector
      util::NormalDistribution<double>rand_vec(n, 0.0, 1.0, 1);
      std::vector<double> rand_vec2(rand_vec.data());

      // Populate with random numbers
      n = 0;
      if (field.rank() == 2) {
        auto view = atlas::array::make_view<double, 2>(field);
        if (functionSpace.type() == "Spectral") {
          atlas::functionspace::Spectral fs(functionSpace);
          const atlas::idx_t N = fs.truncation();
          const auto zonal_wavenumbers = fs.zonal_wavenumbers();
          const atlas::idx_t nb_zonal_wavenumbers = zonal_wavenumbers.size();
          int jnode = 0;
          for (int jm=0; jm < nb_zonal_wavenumbers; ++jm) {
            const atlas::idx_t m1 = zonal_wavenumbers(jm);
            for (std::size_t n1 = m1; n1 <= static_cast<std::size_t>(N); ++n1) {
              if (m1 == 0) {
                // Real part only
                for (atlas::idx_t jlevel = 0; jlevel < field.shape(1); ++jlevel) {
                  view(jnode, jlevel) = rand_vec[n];
                  ++n;
                }
                ++jnode;

                // No imaginary part
                for (atlas::idx_t jlevel = 0; jlevel < field.shape(1); ++jlevel) {
                  view(jnode, jlevel) = 0.0;
                }
                ++jnode;
              } else {
                // Real part
                for (atlas::idx_t jlevel = 0; jlevel < field.shape(1); ++jlevel) {
                  view(jnode, jlevel) = rand_vec2[n];
                  ++n;
                }
                ++jnode;

                // Imaginary part
                for (atlas::idx_t jlevel = 0; jlevel < field.shape(1); ++jlevel) {
                  view(jnode, jlevel) = rand_vec2[n];
                  ++n;
                }
                ++jnode;
              }
            }
          }
        } else {
          for (atlas::idx_t jnode = 0; jnode < field.shape(0); ++jnode) {
            for (atlas::idx_t jlevel = 0; jlevel < field.shape(1); ++jlevel) {
              if (ghostView(jnode) == 0) {
                view(jnode, jlevel) = rand_vec2[n];
                ++n;
              } else {
                view(jnode, jlevel) = 0.0;
              }
            }
          }
        }
      }

      // Add field
      fset.add(field);
    }

    if (functionSpace.type() != "Spectral") {
      // Halo exchange
      fset.haloExchange();
    }

    // Return FieldSet
    return fset;
  }

  // -----------------------------------------------------------------------------
  // TODO(Benjamin): should be moved in OOPS?
  atlas::FieldSet copyFieldSet(const atlas::FieldSet & otherFset) const {
    // Create FieldSet
    atlas::FieldSet fset;

    for (const auto & otherField : otherFset) {
      // Create Field
      atlas::Field field = otherField.functionspace().createField<double>(
        atlas::option::name(otherField.name()) | atlas::option::levels(otherField.levels()));

      // Copy data
      if (field.rank() == 2) {
        auto view = atlas::array::make_view<double, 2>(field);
        auto otherView = atlas::array::make_view<double, 2>(otherField);
        for (atlas::idx_t jnode = 0; jnode < otherField.shape(0); ++jnode) {
          for (atlas::idx_t jlevel = 0; jlevel < otherField.shape(1); ++jlevel) {
              view(jnode, jlevel) = otherView(jnode, jlevel);
          }
        }
      }

      // Add field
      fset.add(field);
    }

    // Return FieldSet
    return fset;
  }

  // -----------------------------------------------------------------------------
  // TODO(Benjamin): should be moved in OOPS?
  double dot_product(const atlas::FieldSet & fset1,
                     const atlas::FieldSet & fset2,
                     const eckit::mpi::Comm & comm) const {
    // Check FieldSets size
    ASSERT(fset1.size() == fset2.size());

    // Compute dot product
    double dp = 0.0;
    for (const auto & field1 : fset1) {
      if (field1.rank() == 2) {
        atlas::Field field2 = fset2.field(field1.name());

        // Check fields consistency
        ASSERT(field2.rank() == 2);
        ASSERT(field1.shape(0) == field2.shape(0));
        ASSERT(field1.shape(1) == field2.shape(1));

        // Add contributions
        auto view1 = atlas::array::make_view<double, 2>(field1);
        auto view2 = atlas::array::make_view<double, 2>(field2);
        if (field1.functionspace().type() == "Spectral") {
          atlas::functionspace::Spectral fs(field1.functionspace());
          const atlas::idx_t N = fs.truncation();
          const auto zonal_wavenumbers = fs.zonal_wavenumbers();
          const atlas::idx_t nb_zonal_wavenumbers = zonal_wavenumbers.size();
          int jnode = 0;
          for (int jm=0; jm < nb_zonal_wavenumbers; ++jm) {
            const atlas::idx_t m1 = zonal_wavenumbers(jm);
            for (std::size_t n1 = m1; n1 <= static_cast<std::size_t>(N); ++n1) {
              if (m1 == 0) {
                // Real part only
                for (atlas::idx_t jlevel = 0; jlevel < field1.shape(1); ++jlevel) {
                  dp += view1(jnode, jlevel)*view2(jnode, jlevel);
                }
                ++jnode;

                // No imaginary part
                ++jnode;
              } else {
                // Real part
                for (atlas::idx_t jlevel = 0; jlevel < field1.shape(1); ++jlevel) {
                  dp += 2.0*view1(jnode, jlevel)*view2(jnode, jlevel);
                }
                ++jnode;

                // Imaginary part
                for (atlas::idx_t jlevel = 0; jlevel < field1.shape(1); ++jlevel) {
                  dp += 2.0*view1(jnode, jlevel)*view2(jnode, jlevel);
                }
                ++jnode;
              }
            }
          }
        } else {
          for (atlas::idx_t jnode = 0; jnode < field1.shape(0); ++jnode) {
            for (atlas::idx_t jlevel = 0; jlevel < field1.shape(1); ++jlevel) {
              dp += view1(jnode, jlevel)*view2(jnode, jlevel);
            }
          }
        }
      }
    }

    // Allreduce
    comm.allReduceInPlace(dp, eckit::mpi::sum());

    // Return dot product
    return dp;
  }
};

// -----------------------------------------------------------------------------

}  // namespace saber
