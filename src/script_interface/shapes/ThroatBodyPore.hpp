/*
  Copyright (C) 2017-2018 The ESPResSo project

  This file is part of ESPResSo.

  ESPResSo is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  ESPResSo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SCRIPT_INTERFACE_SHAPES_THROAT_BODY_PORE_HPP
#define SCRIPT_INTERFACE_SHAPES_THROAT_BODY_PORE_HPP

#include "Shape.hpp"
#include "core/shapes/ThroatBodyPore.hpp"

namespace ScriptInterface {
namespace Shapes {

class ThroatBodyPore : public Shape {
  using CoreShape = ::Shapes::ThroatBodyPore;
  std::shared_ptr<::Shapes::ThroatBodyPore> m_throat_body_pore;

public:
  ThroatBodyPore() : m_throat_body_pore(new ::Shapes::ThroatBodyPore()) {
    add_parameters(
        {{"radius", m_throat_body_pore, &CoreShape::set_radius, &CoreShape::radius},
         {"length", m_throat_body_pore, &CoreShape::set_length, &CoreShape::length},
         {"center", m_throat_body_pore, &CoreShape::set_center, &CoreShape::center},
         {"width", m_throat_body_pore, &CoreShape::set_width, &CoreShape::width}});
  }

  std::shared_ptr<::Shapes::Shape> shape() const override {
    return m_throat_body_pore;
  }
};
} // namespace Shapes
} // namespace ScriptInterface

#endif
