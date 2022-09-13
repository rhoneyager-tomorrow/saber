/*
 * (C) Crown Copyright 2022 Met Office
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#ifndef SABER_VADER_MOISTURECONTROLSABERBLOCK_H_
#define SABER_VADER_MOISTURECONTROLSABERBLOCK_H_

#include <memory>
#include <string>
#include <vector>

#include "atlas/field.h"
#include "atlas/functionspace.h"

#include "eckit/exception/Exceptions.h"

#include "oops/base/Variables.h"

#include "saber/oops/SaberBlockBase.h"
#include "saber/oops/SaberBlockParametersBase.h"
#include "saber/vader/MoistureControlParameters.h"

namespace oops {
  class Variables;
}

namespace saber {

// -----------------------------------------------------------------------------
class MoistureControlSaberBlockParameters : public SaberBlockParametersBase {
  OOPS_CONCRETE_PARAMETERS(MoistureControlSaberBlockParameters, SaberBlockParametersBase)
 public:
  oops::RequiredParameter<moisturecontrolParameters>
    moisturecontrolParams{"covariance data", this};
};

// -----------------------------------------------------------------------------
// This saber block is here
//
// -----------------------------------------------------------------------------
class MoistureControlSaberBlock : public SaberBlockBase {
 public:
  static const std::string classname() {return "saber::MoistureControlSaberBlock";}

  typedef MoistureControlSaberBlockParameters Parameters_;

  MoistureControlSaberBlock(const atlas::FunctionSpace &,
                            const atlas::FieldSet &,
                            const std::vector<size_t> &,
                            const Parameters_ &,
                            const atlas::FieldSet &,
                            const atlas::FieldSet &,
                            const std::vector<atlas::FieldSet> &);
  virtual ~MoistureControlSaberBlock();

  void randomize(atlas::FieldSet &) const override;
  void multiply(atlas::FieldSet &) const override;
  void inverseMultiply(atlas::FieldSet &) const override;
  void multiplyAD(atlas::FieldSet &) const override;
  void inverseMultiplyAD(atlas::FieldSet &) const override;

 private:
  void print(std::ostream &) const override;
  oops::Variables inputVars_;
  atlas::FieldSet covFieldSet_;
  atlas::FieldSet augmentedStateFieldSet_;
};

// -----------------------------------------------------------------------------

}  // namespace saber

#endif  // SABER_VADER_MOISTURECONTROLSABERBLOCK_H_

