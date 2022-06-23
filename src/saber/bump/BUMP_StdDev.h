/*
 * (C) Copyright 2021 UCAR
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#ifndef SABER_BUMP_BUMP_STDDEV_H_
#define SABER_BUMP_BUMP_STDDEV_H_

#include <memory>
#include <string>
#include <vector>

#include "atlas/field.h"

#include "oops/base/Geometry.h"
#include "oops/base/Variables.h"

#include "saber/bump/BUMP.h"
#include "saber/oops/SaberBlockBase.h"
#include "saber/oops/SaberBlockParametersBase.h"

namespace oops {
  class Variables;
}

namespace saber {

// -----------------------------------------------------------------------------
template <typename MODEL>
class BUMP_StdDevParameters : public SaberBlockParametersBase {
  OOPS_CONCRETE_PARAMETERS(BUMP_StdDevParameters, SaberBlockParametersBase)

 public:
  oops::RequiredParameter<BUMP_Parameters<MODEL>> bumpParams{"bump", this};
};

// -----------------------------------------------------------------------------


template <typename MODEL>
class BUMP_StdDev : public SaberBlockBase<MODEL> {
  typedef oops::Geometry<MODEL> Geometry_;
  typedef oops::State<MODEL>    State_;
  typedef BUMP<MODEL>           BUMP_;

 public:
  static const std::string classname() {return "saber::BUMP_StdDev";}

  typedef BUMP_StdDevParameters<MODEL> Parameters_;

  BUMP_StdDev(const Geometry_ &,
              const Parameters_ &,
              const State_ &,
              const State_ &);
  virtual ~BUMP_StdDev();

  void randomize(atlas::FieldSet &) const override;
  void multiply(atlas::FieldSet &) const override;
  void inverseMultiply(atlas::FieldSet &) const override;
  void multiplyAD(atlas::FieldSet &) const override;
  void inverseMultiplyAD(atlas::FieldSet &) const override;

 private:
  void print(std::ostream &) const override;
  std::unique_ptr<BUMP_> bump_;
};

// -----------------------------------------------------------------------------

template<typename MODEL>
BUMP_StdDev<MODEL>::BUMP_StdDev(const Geometry_ & geom,
                                const Parameters_ & params,
                                const State_ & xb,
                                const State_ & fg)
  : SaberBlockBase<MODEL>(params), bump_()
{
  oops::Log::trace() << classname() << "::BUMP_StdDev starting" << std::endl;

  // Setup and check input/ouput variables
  const oops::Variables inputVars = params.inputVars.value();
  const oops::Variables outputVars = params.outputVars.value();
  ASSERT(inputVars == outputVars);

  // Active variables
  const boost::optional<oops::Variables> &activeVarsPtr = params.activeVars.value();
  oops::Variables activeVars;
  if (activeVarsPtr != boost::none) {
    activeVars += *activeVarsPtr;
    ASSERT(activeVars <= inputVars);
  } else {
    activeVars += inputVars;
  }

  // Initialize BUMP
  bump_.reset(new BUMP_(geom, geom, activeVars, params.bumpParams.value(), xb, fg));

  oops::Log::trace() << classname() << "::BUMP_StdDev done" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
BUMP_StdDev<MODEL>::~BUMP_StdDev() {
  oops::Log::trace() << classname() << "::~BUMP_StdDev starting" << std::endl;
  util::Timer timer(classname(), "~BUMP_StdDev");
  oops::Log::trace() << classname() << "::~BUMP_StdDev done" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
void BUMP_StdDev<MODEL>::randomize(atlas::FieldSet & fset) const {
  oops::Log::trace() << classname() << "::randomize starting" << std::endl;
  this->multiply(fset);
  oops::Log::trace() << classname() << "::randomize done" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
void BUMP_StdDev<MODEL>::multiply(atlas::FieldSet & fset) const {
  oops::Log::trace() << classname() << "::multiply starting" << std::endl;
  bump_->multiplyStdDev(fset);
  oops::Log::trace() << classname() << "::multiply done" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
void BUMP_StdDev<MODEL>::inverseMultiply(atlas::FieldSet & fset) const {
  oops::Log::trace() << classname() << "::inverseMultiply starting" << std::endl;
  bump_->inverseMultiplyStdDev(fset);
  oops::Log::trace() << classname() << "::inverseMultiply done" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
void BUMP_StdDev<MODEL>::multiplyAD(atlas::FieldSet & fset) const {
  oops::Log::trace() << classname() << "::multiplyAD starting" << std::endl;
  this->multiply(fset);
  oops::Log::trace() << classname() << "::multiplyAD done" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
void BUMP_StdDev<MODEL>::inverseMultiplyAD(atlas::FieldSet & fset) const {
  oops::Log::trace() << classname() << "::inverseMultiplyAD starting" << std::endl;
  this->inverseMultiply(fset);
  oops::Log::trace() << classname() << "::inverseMultiplyAD done" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
void BUMP_StdDev<MODEL>::print(std::ostream & os) const {
  os << classname();
}

// -----------------------------------------------------------------------------

}  // namespace saber

#endif  // SABER_BUMP_BUMP_STDDEV_H_
