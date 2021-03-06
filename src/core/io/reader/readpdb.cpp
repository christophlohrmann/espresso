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
#include "readpdb.hpp"
#include "grid.hpp"
#include "nonbonded_interactions/lj.hpp"

#include <cmath>
#include <iostream>
#include <set>

namespace Reader {
namespace PDB {

#ifdef LENNARD_JONES
/* Add user requested Lennard-Jones interactions */
static void add_lj_interaction(
    std::set<PdbParser::itp_atomtype, PdbParser::itp_atomtype_compare> &types,
    std::vector<PdbLJInteraction> interactions, const double rel_cutoff) {
  for (std::vector<PdbLJInteraction>::const_iterator it = interactions.begin();
       it != interactions.end(); ++it) {
    for (auto type : types) {
      const double epsilon_ij = sqrt(it->epsilon * type.epsilon);
      const double sigma_ij = 0.5 * (it->sigma + 10. * type.sigma);
      const double cutoff_ij = rel_cutoff * sigma_ij;
      const double shift_ij =
          -(pow(sigma_ij / cutoff_ij, 12) - pow(sigma_ij / cutoff_ij, 6));
      if ((epsilon_ij <= 0) || (sigma_ij <= 0)) {
        continue;
      }
      lennard_jones_set_params(it->other_type, type.espresso_id, epsilon_ij,
                               sigma_ij, cutoff_ij, shift_ij, 0.0, 0.0);
    }
  }
}

/* Add Lennard-Jones interactions between particles added from pdb/itp file */
static void add_lj_internal(
    std::set<PdbParser::itp_atomtype, PdbParser::itp_atomtype_compare> &types,
    const double rel_cutoff, bool only_diagonal) {
  for (auto it = types.begin(); it != types.end(); ++it) {
    for (auto type : types) {
      if (it->espresso_id > type.espresso_id)
        continue;
      if (only_diagonal && (it->espresso_id != type.espresso_id))
        continue;
      const double epsilon_ij = sqrtf(it->epsilon * type.epsilon);
      const double sigma_ij = 0.5 * (10. * it->sigma + 10. * type.sigma);
      const double cutoff_ij = rel_cutoff * sigma_ij;
      const double shift_ij =
          -pow(sigma_ij / cutoff_ij, 12) - pow(sigma_ij / cutoff_ij, 6);
      if ((epsilon_ij <= 0) || (sigma_ij <= 0)) {
        continue;
      }
      lennard_jones_set_params(it->espresso_id, type.espresso_id, epsilon_ij,
                               sigma_ij, cutoff_ij, shift_ij, 0.0, 0.0);
    }
  }
}
#endif /* LENNARD_JONES */

static int
add_particles(PdbParser::PdbParser &parser, int first_id, int default_type,
              std::set<PdbParser::itp_atomtype, PdbParser::itp_atomtype_compare>
                  &seen_types,
              int first_type = 0, bool fit = false) {
  double pos[3];
  int id = first_id;
  int stat;
  int type;
#ifdef ELECTROSTATICS
  double q;
#endif
  PdbParser::BoundingBox bb;
  bb.llx = bb.lly = bb.llz = 0.0;
  double bb_l[3] = {box_geo.length()[0], box_geo.length()[1],
                    box_geo.length()[2]};

  bb = parser.calc_bounding_box();

  if (fit) {
    bb_l[0] = (bb.urx - bb.llx);
    bb_l[1] = (bb.ury - bb.lly);
    bb_l[2] = (bb.urz - bb.llz);

    for (int i = 0; i < 3; i++) {
      if (bb_l[i] > box_geo.length()[i]) {
        rescale_boxl(i, bb_l[i]);
      }
    }
  }

  int last_type = first_type;
  for (std::vector<PdbParser::pdb_atom>::const_iterator it =
           parser.pdb_atoms.begin();
       it != parser.pdb_atoms.end(); ++it) {
    pos[0] = (it->x - bb.llx);
    pos[1] = (it->y - bb.lly);
    pos[2] = (it->z - bb.llz);

    stat = place_particle(id, pos);

    const std::map<int, PdbParser::itp_atom>::const_iterator entry =
        parser.itp_atoms.find(it->i);
    switch (stat) {
    case ES_PART_OK:
      std::cerr << "Warning: position and type of particle " << id
                << " was overwritten by value from pdb file." << std::endl;
      /* Fall through */
    case ES_PART_CREATED:
      /* See if we have a type from itp file, otherwise set default type */
      if (entry != parser.itp_atoms.end()) {
        PdbParser::itp_atomtype itp_atomtype =
            parser.itp_atomtypes[entry->second.type];
        /* See if we have seen that type before */
        auto type_iterator = seen_types.find(itp_atomtype);
        if (type_iterator == seen_types.end()) {
          itp_atomtype.espresso_id = last_type++;
          type_iterator = seen_types.insert(itp_atomtype).first;
        }
        itp_atomtype = *type_iterator;
#ifdef ELECTROSTATICS
        q = entry->second.charge;
#endif
        type = itp_atomtype.espresso_id;
      } else {
        type = default_type;
#ifdef ELECTROSTATICS
        q = 0;
#endif
      }
      set_particle_type(id, type);
#ifdef ELECTROSTATICS
      set_particle_q(id, q);
#endif
      id++;
      break;
    case ES_PART_ERROR:
      std::cerr << "Warning: Illegal particle id " << id << std::endl;
      return id - first_id;
      break;
    }
  }
  return id - first_id;
}

int pdb_add_particles_from_file(char *pdb_file, int first_id, int type,
                                std::vector<PdbLJInteraction> &ljInteractions,
                                double lj_rel_cutoff, char *itp_file,
                                int first_type, bool fit, bool lj_internal,
                                bool lj_diagonal) {
  int n_part;
  PdbParser::PdbParser parser;
  if (!parser.parse_pdb_file(pdb_file))
    return 0;

  if (itp_file) {
    if (!parser.parse_itp_file(itp_file))
      return 0;
  }

  /* Unique set of types that actually have particles */
  std::set<PdbParser::itp_atomtype, PdbParser::itp_atomtype_compare> seen_types;

  n_part = add_particles(parser, first_id, type, seen_types, first_type, fit);

#ifdef LENNARD_JONES
  add_lj_interaction(seen_types, ljInteractions, lj_rel_cutoff);
  if (lj_internal || lj_diagonal)
    add_lj_internal(seen_types, lj_rel_cutoff, lj_diagonal);
#endif

  return n_part;
}

} // namespace PDB
} // namespace Reader
