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

#include "atlas/field.h"

#include "oops/base/Geometry.h"
#include "oops/base/Increment.h"
#include "oops/base/Variables.h"
#include "oops/util/DateTime.h"
#include "oops/util/Logger.h"

namespace eckit {
  class LocalConfiguration;
}

namespace oops {
  class Variables;
}

namespace saber {

// -------------------------------------------------------------------------------------------------

template<typename MODEL>
std::vector<atlas::FieldSet> readInputFields(
  const oops::Geometry<MODEL> & resol,
  const oops::Variables & vars,
  const util::DateTime & date,
  const boost::optional<std::vector<eckit::LocalConfiguration>> & inputFields) {
  // Vector of FieldSets
  std::vector<atlas::FieldSet> fsetVec;

  // Read block input fields function
  if (inputFields != boost::none) {
    if (inputFields->size() > 0) {
      // Loop over block input fields
      for (const auto & inputField : *inputFields) {
        // Read block input field as Increment
        // TODO(Benjamin): here we assume that all input fields have the background geometry
        // and block input variables
        oops::Increment<MODEL> dx(resol, vars, date);
        eckit::LocalConfiguration file = inputField.getSubConfiguration("file");
        dx.read(file);

        // Define FieldSet name
        std::string name = inputField.getString("parameter");
        if (inputField.has("component")) {
          name += "::" + std::to_string(inputField.getInt("component"));
        }

        // Transform Increment into FieldSet
        oops::Log::test() << "Norm of input parameter " << name << ": " << dx.norm() << std::endl;
        atlas::FieldSet fset;
        fset.name() = name;
        for (const atlas::Field field : dx.fieldSet()) {
          fset.add(field);
        }
        fsetVec.push_back(fset);
      }
    }
  }
  return fsetVec;
}

// -----------------------------------------------------------------------------

}  // namespace saber