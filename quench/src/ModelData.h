/*
 * (C) Copyright 2023 UCAR.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#pragma once

#include <ostream>
#include <string>

#include "oops/util/Printable.h"

namespace quench {
  class Geometry;

// -------------------------------------------------------------------------------------------------

class ModelData : public util::Printable {
 public:
  static const std::string classname() {return "quench::ModelData";}

  explicit ModelData(const Geometry &);
  ~ModelData();

  const eckit::LocalConfiguration modelData() const;

 private:
  void print(std::ostream &) const override;
};

// -------------------------------------------------------------------------------------------------

}  // namespace quench
