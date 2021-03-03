/*
 * Copyright (C) 2010-2019 The ESPResSo project
 * Copyright (C) 2002,2003,2004,2005,2006,2007,2008,2009,2010
 *   Max-Planck-Institute for Polymer Research, Theory Group
 *
 * This file is part of ESPResSo.
 *
 * ESPResSo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ESPResSo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCRIPT_INTERFACE_OBSERVABLES_CYLINDRICALLBPROFILEOBSERVABLE_HPP
#define SCRIPT_INTERFACE_OBSERVABLES_CYLINDRICALLBPROFILEOBSERVABLE_HPP

#include "script_interface/auto_parameters/AutoParameters.hpp"

#include "Observable.hpp"
#include "core/observables/CylindricalLBProfileObservable.hpp"
#include "script_interface/get_value.hpp"

#include "script_interface/CylTrafoParams.hpp"

#include <boost/range/algorithm.hpp>

#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#include <vector>

namespace ScriptInterface {
namespace Observables {

template <typename CoreCylLBObs>
class CylindricalLBProfileObservable
    : public AutoParameters<CylindricalLBProfileObservable<CoreCylLBObs>,
                            Observable> {
  using Base =
      AutoParameters<CylindricalLBProfileObservable<CoreCylLBObs>, Observable>;

public:
  static_assert(std::is_base_of<::Observables::CylindricalLBProfileObservable,
                                CoreCylLBObs>::value,
                "");
  using Base::Base;
  CylindricalLBProfileObservable() {
    this->add_parameters({
        {"cyl_tafo_params", Utils::as_const(m_cyl_trafo_params)},
        {"n_r_bins",
         [this](const Variant &v) {
           cylindrical_profile_observable()->n_bins[0] =
               static_cast<size_t>(get_value<int>(v));
         },
         [this]() {
           return static_cast<int>(cylindrical_profile_observable()->n_bins[0]);
         }},
        {"n_phi_bins",
         [this](const Variant &v) {
           cylindrical_profile_observable()->n_bins[1] =
               static_cast<size_t>(get_value<int>(v));
         },
         [this]() {
           return static_cast<int>(cylindrical_profile_observable()->n_bins[1]);
         }},
        {"n_z_bins",
         [this](const Variant &v) {
           cylindrical_profile_observable()->n_bins[2] =
               static_cast<size_t>(get_value<int>(v));
         },
         [this]() {
           return static_cast<int>(cylindrical_profile_observable()->n_bins[2]);
         }},
        {"min_r",
         [this](const Variant &v) {
           cylindrical_profile_observable()->limits[0].first =
               get_value<double>(v);
         },
         [this]() {
           return cylindrical_profile_observable()->limits[0].first;
         }},
        {"min_phi",
         [this](const Variant &v) {
           cylindrical_profile_observable()->limits[1].first =
               get_value<double>(v);
         },
         [this]() {
           return cylindrical_profile_observable()->limits[1].first;
         }},
        {"min_z",
         [this](const Variant &v) {
           cylindrical_profile_observable()->limits[2].first =
               get_value<double>(v);
         },
         [this]() {
           return cylindrical_profile_observable()->limits[2].first;
         }},
        {"max_r",
         [this](const Variant &v) {
           cylindrical_profile_observable()->limits[0].second =
               get_value<double>(v);
         },
         [this]() {
           return cylindrical_profile_observable()->limits[0].second;
         }},
        {"max_phi",
         [this](const Variant &v) {
           cylindrical_profile_observable()->limits[1].second =
               get_value<double>(v);
         },
         [this]() {
           return cylindrical_profile_observable()->limits[1].second;
         }},
        {"max_z",
         [this](const Variant &v) {
           cylindrical_profile_observable()->limits[2].second =
               get_value<double>(v);
         },
         [this]() {
           return cylindrical_profile_observable()->limits[2].second;
         }},
        {"sampling_density",
         [this](const Variant &v) {
           cylindrical_profile_observable()->sampling_density =
               get_value<double>(v);
         },
         [this]() {
           return cylindrical_profile_observable()->sampling_density;
         }},
    });
  }

  void do_construct(VariantMap const &params) override {
      set_from_args(m_cyl_trafo_params, params, "cyl_trafo_params");
      m_observable = std::make_shared<CoreCylLBObs>(
          m_cyl_trafo_params->cyl_trafo_params(),
          get_value_or<int>(params, "n_r_bins", 1),
          get_value_or<int>(params, "n_phi_bins", 1),
          get_value_or<int>(params, "n_z_bins", 1),
          get_value_or<double>(params, "min_r", 0.),
          get_value<double>(params, "max_r"),
          get_value_or<double>(params, "min_phi", -Utils::pi()),
          get_value_or<double>(params, "max_phi", Utils::pi()),
          get_value<double>(params, "min_z"),
          get_value<double>(params, "max_z"));
    }

  Variant do_call_method(std::string const &method,
                         VariantMap const &parameters) override {
    if (method == "edges") {
      std::vector<Variant> variant_edges;
      boost::copy(cylindrical_profile_observable()->edges(),
                  std::back_inserter(variant_edges));
      return variant_edges;
    }
    return Base::do_call_method(method, parameters);
  }

  std::shared_ptr<::Observables::Observable> observable() const override {
    return m_observable;
  }

  virtual std::shared_ptr<::Observables::CylindricalLBProfileObservable>
  cylindrical_profile_observable() const {
    return m_observable;
  }

private:
  std::shared_ptr<CoreCylLBObs> m_observable;
  std::shared_ptr<CylTrafoParams> m_cyl_trafo_params;
};

} /* namespace Observables */
} /* namespace ScriptInterface */

#endif
