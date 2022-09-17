/*
 * (C) Copyright 2021 UCAR
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#ifndef SABER_OOPS_LOCALIZATION_H_
#define SABER_OOPS_LOCALIZATION_H_

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "atlas/field.h"

#include "eckit/config/Configuration.h"

#include "oops/base/Variables.h"
#include "oops/generic/LocalizationBase.h"
#include "oops/util/Duration.h"
#include "oops/util/Logger.h"

#include "saber/oops/ReadInputFields.h"
#include "saber/oops/SaberBlockBase.h"
#include "saber/oops/SaberBlockParametersBase.h"

namespace eckit {
  class Configuration;
}

namespace oops {
  class Variables;
}

namespace saber {

// -----------------------------------------------------------------------------

template<typename MODEL>
class Localization : public oops::LocalizationBase<MODEL> {
  typedef oops::Geometry<MODEL>               Geometry_;
  typedef oops::Increment<MODEL>              Increment_;
  typedef oops::State<MODEL>                  State_;

 public:
  Localization(const Geometry_ &,
               const oops::Variables &,
               const eckit::Configuration &);
  ~Localization();

  void randomize(Increment_ &) const override;
  void multiply(Increment_ &) const override;

 private:
  void print(std::ostream &) const override;
  std::unique_ptr<SaberBlockBase> saberBlock_;
};

// =============================================================================

template<typename MODEL>
Localization<MODEL>::Localization(const Geometry_ & resol,
                                  const oops::Variables & vars,
                                  const eckit::Configuration & conf)
  : saberBlock_()
{
  oops::Log::trace() << "Localization::Localization starting" << std::endl;

  size_t myslot = resol.timeComm().rank();
  if (myslot == 0) {
    // Get parameters from configuration
    const eckit::LocalConfiguration saberBlock(conf, "saber block");
    SaberBlockParametersWrapper saberBlockParamWrapper;
    saberBlockParamWrapper.validateAndDeserialize(saberBlock);

    const SaberBlockParametersBase & saberBlockParams = saberBlockParamWrapper.saberBlockParameters;

    // Create dummy FieldSet (for xb and fg)
    atlas::FieldSet dummyFs;

    // Create dummy time
    util::DateTime dummyTime(1977, 5, 25, 0, 0, 0);

    // Get block input fields
    std::vector<atlas::FieldSet> fsetVec = readInputFields(
      resol,
      saberBlockParams.inputVars.value(),
      dummyTime,
      saberBlockParams.inputFields.value());

    // Create SABER block
    saberBlock_.reset(SaberBlockFactory::create(resol.getComm(),
                      resol.functionSpace(),
                      resol.extraFields(),
                      resol.variableSizes(saberBlockParams.inputVars.value()),
                      saberBlockParams,
                      dummyFs,
                      dummyFs,
                      fsetVec));
  }

  oops::Log::trace() << "Localization:Localization done" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
Localization<MODEL>::~Localization() {
  oops::Log::trace() << "Localization:~Localization destructed" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
void Localization<MODEL>::randomize(Increment_ & dx) const {
  oops::Log::trace() << "Localization:randomize starting" << std::endl;

  // Random output vector (necessary for some SABER blocks)
  dx.random();

  // Central block randomization
  saberBlock_->randomize(dx.fieldSet());

  // ATLAS fieldset to Increment_
  dx.synchronizeFields();

  oops::Log::trace() << "Localization:randomize done" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
void Localization<MODEL>::multiply(Increment_ & dx) const {
  oops::Log::trace() << "Localization:multiply starting" << std::endl;

  // Central block multiplication
  saberBlock_->multiply(dx.fieldSet());

  // ATLAS fieldset to Increment_
  dx.synchronizeFields();

  oops::Log::trace() << "Localization:multiply done" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
void Localization<MODEL>::print(std::ostream & os) const {
  os << "Localization:print not implemeted yet";
}

// -----------------------------------------------------------------------------

}  // namespace saber

#endif  // SABER_OOPS_LOCALIZATION_H_
