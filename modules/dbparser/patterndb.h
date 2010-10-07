/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef PATTERNDB_H_INCLUDED
#define PATTERNDB_H_INCLUDED

#include "radix.h"
#include "templates.h"
#include "timerwheel.h"
#include "filter.h"

typedef struct _PatternDB PatternDB;

/* This class encapsulates a correllation context stored in
   db->state. */
typedef struct _PDBContext
{
  /* key in the hash table*/
  gchar *id;
  /* back reference to the PatternDB */
  PatternDB *db;
  /* timeout timer */
  TWEntry *timer;
  /* messages belonging to this context */
  GPtrArray *messages;
  gint ref_cnt;
} PDBContext;

typedef struct _PDBMessage
{
  GArray *tags;
  GPtrArray *values;
} PDBMessage;

enum
{
  LCS_GLOBAL,
  LCS_HOST,
  LCS_PROGRAM,
};

/* this class encapsulates a the verdict of a rule in the pattern
 * database and is stored as the "value" member in the RADIX tree
 * node. It contains a reference the the original rule in the rule
 * database. */
typedef struct _PDBRule
{
  guint ref_cnt;
  gchar *class;
  gchar *rule_id;
  PDBMessage msg;
  gint context_timeout;
  gint context_scope;
  LogTemplate *context_id_template;
  GPtrArray *actions;
} PDBRule;

/* this class encapsulates an example message in the pattern database
 * used for testing rules and patterns. It contains the message with the
 * program field and the expected rule_id with the expected name/value
 * pairs. */

typedef struct _PDBExample
{
  PDBRule *rule;
  gchar *message;
  gchar *program;
  GPtrArray *values;
} PDBExample;

/*
 * This class encapsulates a set of program related rules in the
 * pattern database. Its instances are stored as "value" in the
 * program name RADIX tree. It basically contains another RADIX for
 * the per-program patterns.
 */
typedef struct _PDBProgram
{
  guint ref_cnt;
  RNode *rules;
} PDBProgram;

struct _PatternDB
{
  RNode *programs;
  gchar *version;
  gchar *pub_date;
  GHashTable *state;
  TimerWheel *timer_wheel;
};

gboolean pattern_db_lookup(PatternDB *self, LogMessage *msg, GSList **dbg_list);
gboolean pattern_db_load(PatternDB *self, GlobalConfig *cfg, const gchar *config, GList **examples);

PatternDB *pattern_db_new(void);
void pattern_db_free(PatternDB *self);

void pdb_example_free(PDBExample *s);

void pattern_db_global_init(void);

#endif
