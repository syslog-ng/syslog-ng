/*
 * Copyright (c) 2002-2013, 2015 Balabit
 * Copyright (c) 1998-2013, 2015 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#ifndef PATTERNDB_PDB_RULE_H_INCLUDED
#define PATTERNDB_PDB_RULE_H_INCLUDED

#include "syslog-ng.h"
#include "pdb-action.h"

/* this class encapsulates a the verdict of a rule in the pattern
 * database and is stored as the "value" member in the RADIX tree
 * node. It contains a reference to the original rule in the rule
 * database. */
typedef struct _PDBRule PDBRule;
struct _PDBRule
{
  GAtomicCounter ref_cnt;
  gchar *class;
  gchar *rule_id;
  SyntheticMessage msg;
  SyntheticContext context;
  GPtrArray *actions;
};

void pdb_rule_set_class(PDBRule *self, const gchar *class);
void pdb_rule_set_rule_id(PDBRule *self, const gchar *rule_id);
void pdb_rule_add_action(PDBRule *self, PDBAction *action);
gchar *pdb_rule_get_name(PDBRule *self);

PDBRule *pdb_rule_new(void);
PDBRule *pdb_rule_ref(PDBRule *self);
void pdb_rule_unref(PDBRule *self);

#endif
