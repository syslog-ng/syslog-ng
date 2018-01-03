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
#ifndef DBPARSER_PDB_ACTION_H_INCLUDED
#define DBPARSER_PDB_ACTION_H_INCLUDED

#include "syslog-ng.h"
#include "synthetic-message.h"
#include "synthetic-context.h"
#include "filter/filter-expr.h"

/* rule action triggers */
typedef enum
{
  RAT_MATCH = 1,
  RAT_TIMEOUT
} PDBActionTrigger;

/* action content*/
typedef enum
{
  RAC_NONE,
  RAC_MESSAGE,
  RAC_CREATE_CONTEXT,
} PDBActionContentType;

/* a rule may contain one or more actions to be performed */
typedef struct _PDBAction
{
  FilterExprNode *condition;
  PDBActionTrigger trigger;
  PDBActionContentType content_type;
  guint32 rate_quantum;
  guint16 rate;
  guint8 id;
  union
  {
    SyntheticMessage message;
    struct
    {
      SyntheticMessage message;
      SyntheticContext context;
    } create_context;
  } content;
} PDBAction;

void pdb_action_set_condition(PDBAction *self, GlobalConfig *cfg, const gchar *filter_string, GError **error);
void pdb_action_set_rate(PDBAction *self, const gchar *rate_);
void pdb_action_set_trigger(PDBAction *self, const gchar *trigger, GError **error);

PDBAction *pdb_action_new(gint id);
void pdb_action_free(PDBAction *self);

#endif
