/*
  Copyright (C) 2010-2018 The ESPResSo project
  Copyright (C) 2002,2003,2004,2005,2006,2007,2008,2009,2010
  Max-Planck-Institute for Polymer Research, Theory Group

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

#ifndef __THROATBODYPORE_HPP
#define __THROATBODYPORE_HPP

#include "Shape.hpp"
#include <utils/Vector.hpp>

namespace Shapes {
class ThroatBodyPore : public Shape {
public:
  explicit ThroatBodyPore() = default;

  void calculate_dist(const Utils::Vector3d &pos, double &dist,
                      Utils::Vector3d &vec) const override;

  double radius() const { return m_radius; }
  void set_radius(double const &radius) { m_radius = radius; }
  double length() const { return m_length; }
  void set_length(double const &length) { m_length = length; }
  double width() const { return m_width; }
  void set_width(double const &width) { m_width = width; }
  Utils::Vector3d const &center() const { return m_center; }
  void set_center(Utils::Vector3d const &center) { m_center = center; }
  
private:
  double m_radius;
  double m_width;
  double m_length;
  Utils::Vector3d m_center;
};
} // namespace Shapes

#endif
