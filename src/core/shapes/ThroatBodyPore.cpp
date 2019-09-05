/*
Copyright (C) 2010-2018 The ESPResSo project

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

#include "ThroatBodyPore.hpp"

#include <cassert>

namespace Shapes {

void ThroatBodyPore::calculate_dist(const Utils::Vector3d &pos, double &dist,
                                Utils::Vector3d &vec) const {
    //does not calculate distance but just gives a flag if in or out
    Utils::Vector3d pos_rel = pos - m_center;
    
    vec = Utils::Vector3d({0,0,1});
    dist = -1;
    if (pos_rel[0] > m_length/2. || pos_rel[0]< -m_length/2.)
        dist = 1;
    if (std::abs(pos_rel[1])<m_width/2.)
        dist = 1;
    if (pos_rel.norm() < m_radius)
        dist = 1;
    return;
}
} // namespace Shapes
