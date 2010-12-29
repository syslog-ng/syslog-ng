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
 * Internal declarations to be used by pdbtool/patterndb internals. Not a
 * public API.
 *
 */
#ifndef PATTERNDB_INT_H_INCLUDED
#define PATTERNDB_INT_H_INCLUDED

#include "patterndb.h"

typedef struct _PDBRule PDBRule;

/* rule context scope */
enum
{
  /* correllation happens globally, e.g. log messages even on different hosts are considered */
  RCS_GLOBAL,
  /* correllation happens inside the same host only, e.g. messages from other hosts are not considered */
  RCS_HOST,
  /* correllation happens for the same program only, e.g. messages from other programs are not considered */
  RCS_PROGRAM,
  /* correllation happens for the same process only, e.g. messages from a different program/pid are not considered */
  RCS_PROCESS,
};

/* type field for state key */
enum
{
  /* state entry contains a context */
  PSK_CONTEXT,
  /* state entry contains a ratelimit state */
  PSK_RATE_LIMIT,
};

typedef struct _PDBStateKey
{
  const gchar *host;
  const gchar *program;
  const gchar *pid;
  gchar *session_id;
  guint8 scope;
  guint8 type;
} PDBStateKey;

/* This class encapsulates a correllation context, keyed by PDBStateKey, type == PSK_RULE. */
typedef struct _PDBContext
{
  /* key in the hashtable. NOTE: host/program/pid/session_id are borrowed pointers from the first message in the state */
  PDBStateKey key;
  /* back reference to the PatternDB */
  PatternDB *db;
  /* back reference to the last rule touching this context */
  PDBRule *rule;
  /* timeout timer */
  TWEntry *timer;
  /* messages belonging to this context */
  GPtrArray *messages;
  gint ref_cnt;
} PDBContext;

/* This class encapsulates a rate-limit state stored in
   db->state. */
typedef struct _PDBRateLimit
{
  /* key in the hashtable. NOTE: host/program/pid/session_id are allocated, thus they need to be freed when the structure is freed. */
  PDBStateKey key;
  gint buckets;
  guint64 last_check;
} PDBRateLimit;

typedef struct _PDBStateEntry
{
  union
  {
    PDBStateKey key;
    PDBContext context;
    PDBRateLimit rate_limit;
  };
} PDBStateEntry;

typedef struct _PDBMessage
{
  GArray *tags;
  GPtrArray *values;
} PDBMessage;

/* rule action triggers */
enum
 {
  RAT_MATCH = 1,
  RAT_TIMEOUT
};

/* action content*/
enum
{
  RAC_NONE,
  RAC_MESSAGE
};

/* a rule may contain one or more actions to be performed */
typedef struct _PDBAction
{
  FilterExprNode *condition;
  guint8 trigger;
  guint8 content_type;
  guint16 rate;
  guint32 id:8, rate_quantum:24;
  union
  {
    PDBMessage message;
  } content;
} PDBAction;

/* this class encapsulates a the verdict of a rule in the pattern
 * database and is stored as the "value" member in the RADIX tree
 * node. It contains a reference the the original rule in the rule
 * database. */
struct _PDBRule
{
  GAtomicCounter ref_cnt;
  gchar *class;
  gchar *rule_id;
  PDBMessage msg;
  gint context_timeout;
  gint context_scope;
  LogTemplate *context_id_template;
  GPtrArray *actions;
};

void pdb_rule_unref(PDBRule *self);

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

void pdb_example_free(PDBExample *s);

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

/* rules loaded from a pdb file */
typedef struct _PDBRuleSet
{
  RNode *programs;
  gchar *version;
  gchar *pub_date;
} PDBRuleSet;

gboolean pdb_rule_set_load(PDBRuleSet *self, GlobalConfig *cfg, const gchar *config, GList **examples);
PDBRule *pdb_rule_set_lookup(PDBRuleSet *self, LogMessage *msg, GArray *dbg_list);

PDBRuleSet *pdb_rule_set_new(void);
void pdb_rule_set_free(PDBRuleSet *self);

struct _PatternDB
{
  GStaticRWLock lock;
  PDBRuleSet *ruleset;
  GHashTable *state;
  TimerWheel *timer_wheel;
  GTimeVal last_tick;
  PatternDBEmitFunc emit;
  gpointer emit_data;
};

#endif
