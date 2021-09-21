/*
 * (C) Copyright 2017-2020 UCAR
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#ifndef SABER_OOPS_LOCALIZATIONBUMP_H_
#define SABER_OOPS_LOCALIZATIONBUMP_H_

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "eckit/config/Configuration.h"

#include "oops/base/Variables.h"
#include "oops/generic/LocalizationBase.h"
#include "oops/util/Duration.h"
#include "oops/util/Logger.h"

#include "saber/oops/OoBump.h"
#include "saber/oops/ParametersBUMP.h"

namespace eckit {
  class Configuration;
}

namespace oops {
  class Variables;
}

namespace saber {

// -----------------------------------------------------------------------------
/// BUMP localization matrix.

template<typename MODEL>
class LocalizationBUMP : public oops::LocalizationBase<MODEL> {
  typedef oops::Geometry<MODEL>                           Geometry_;
  typedef oops::Increment<MODEL>                          Increment_;
  typedef OoBump<MODEL>                                   OoBump_;
  typedef ParametersBUMP<MODEL>                           ParametersBUMP_;

 public:
  LocalizationBUMP(const Geometry_ &, const eckit::Configuration &);
  ~LocalizationBUMP();

  void randomize(Increment_ &) const override;
  void multiply(Increment_ &) const override;

 private:
  void print(std::ostream &) const override;

  std::unique_ptr<OoBump_> ooBump_;
};

// =============================================================================

template<typename MODEL>
LocalizationBUMP<MODEL>::LocalizationBUMP(const Geometry_ & resol,
                                          const eckit::Configuration & conf)
  : ooBump_()
{
// Setup variables
  const oops::Variables vars(conf, "localization variables");

  size_t myslot = resol.timeComm().rank();
  if (myslot == 0) {
  // Setup parameters
    ParametersBUMP_ param(resol, vars, vars, conf);

  // Transfer OoBump pointer
    ooBump_.reset(new OoBump_(param.getOoBump()));
  }

  oops::Log::trace() << "LocalizationBUMP:LocalizationBUMP constructed" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
LocalizationBUMP<MODEL>::~LocalizationBUMP() {
  oops::Log::trace() << "LocalizationBUMP:~LocalizationBUMP destructed" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
void LocalizationBUMP<MODEL>::randomize(Increment_ & dx) const {
  oops::Log::trace() << "LocalizationBUMP:randomize starting" << std::endl;
  ooBump_->randomize(dx);
  oops::Log::trace() << "LocalizationBUMP:randomize done" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
void LocalizationBUMP<MODEL>::multiply(Increment_ & dx) const {
  oops::Log::trace() << "LocalizationBUMP:multiply starting" << std::endl;
  ooBump_->multiplyNicas(dx);
  oops::Log::trace() << "LocalizationBUMP:multiply done" << std::endl;
}

// -----------------------------------------------------------------------------

template<typename MODEL>
void LocalizationBUMP<MODEL>::print(std::ostream & os) const {
  os << "LocalizationBUMP:print not implemeted yet";
}

// -----------------------------------------------------------------------------

}  // namespace saber

#endif  // SABER_OOPS_LOCALIZATIONBUMP_H_
