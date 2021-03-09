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

#ifndef SCRIPT_INTERFACE_CYL_TRAFO_PARAMS_HPP
#define SCRIPT_INTERFACE_CYL_TRAFO_PARAMS_HPP

#include "script_interface/ScriptInterface.hpp"

#include "utils/math/cyl_trafo_params.hpp"

namespace ScriptInterface {

class CylTrafoParams : public AutoParameters<CylTrafoParams> {
public:
  CylTrafoParams() : m_cyl_trafo_params(new ::Utils::CylTrafoParams()) {
    add_parameters({{"center",
                             [this](Variant const &v) { m_cyl_trafo_params->set_center(get_value<Utils::Vector3d>(v));},
                        [this]() { return m_cyl_trafo_params->get_center(); }
                    },
                    {"axis",
                        [this](Variant const &v) { m_cyl_trafo_params->set_axis(get_value<Utils::Vector3d>(v));},
                        [this]() { return m_cyl_trafo_params->get_axis(); }
                    },
                    {"orientation",
                        [this](Variant const &v) {m_cyl_trafo_params->set_orientation(get_value<Utils::Vector3d>(v));},
                        [this]() { return m_cyl_trafo_params->get_orientation(); }
                    } });
  }

private:
  std::shared_ptr<::Utils::CylTrafoParams> m_cyl_trafo_params;
};
} // namespace ScriptInterface
#endif
