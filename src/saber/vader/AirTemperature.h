/*
 * (C) Crown Copyright 2022 Met Office
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "atlas/field.h"
#include "atlas/functionspace.h"

#include "eckit/exception/Exceptions.h"

#include "oops/base/GeometryData.h"
#include "oops/base/Variables.h"

#include "saber/oops/SaberBlockParametersBase.h"
#include "saber/oops/SaberOuterBlockBase.h"

namespace oops {
  class Variables;
}

namespace saber {
namespace vader {

// -----------------------------------------------------------------------------

class AirTemperatureParameters : public SaberBlockParametersBase {
  OOPS_CONCRETE_PARAMETERS(AirTemperatureParameters, SaberBlockParametersBase)
 public:
};

// -----------------------------------------------------------------------------

class AirTemperature : public SaberOuterBlockBase {
 public:
  static const std::string classname() {return "saber::vader::AirTemperature";}

  typedef AirTemperatureParameters Parameters_;

  AirTemperature(const oops::GeometryData &,
                 const std::vector<size_t> &,
                 const oops::Variables &,
                 const Parameters_ &,
                 const atlas::FieldSet &,
                 const atlas::FieldSet &,
                 const std::vector<atlas::FieldSet> &);
  virtual ~AirTemperature();

  const oops::GeometryData & inputGeometryData() const override {return inputGeometryData_;}
  const oops::Variables & inputVars() const override {return inputVars_;}

  void multiply(atlas::FieldSet &) const override;
  void multiplyAD(atlas::FieldSet &) const override;
  void calibrationInverseMultiply(atlas::FieldSet &) const override;

 private:
  void print(std::ostream &) const override;
  const oops::GeometryData & inputGeometryData_;
  oops::Variables inputVars_;
  atlas::FieldSet augmentedStateFieldSet_;
};

// -----------------------------------------------------------------------------

}  // namespace vader
}  // namespace saber
