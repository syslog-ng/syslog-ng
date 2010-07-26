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
  
#ifndef LOGPATTERNS_H_INCLUDED
#define LOGPATTERNS_H_INCLUDED

#include "radix.h"


/* this class encapsulates a single rule in the pattern database and
 * is stored as the "value" member in the RADIX tree node. It contains
 * a reference the the original rule in the rule database. */
typedef struct _LogDBResult
{
  gchar *class;
  gchar *rule_id;
  GArray *tags;
  GPtrArray *values;
  guint ref_cnt;
} LogDBResult;

/* this class encapsulates an example message in the pattern database
 * used for testing rules and patterns. It contains the message with the
 * program field and the expected rule_id with the expected name/value
 * pairs. */

typedef struct _LogDBExample
{
  LogDBResult *result;
  gchar *message;
  gchar *program;
  GPtrArray *values;
} LogDBExample;

/*
 * This class encapsulates a set of program related rules in the
 * pattern database. Its instances are stored as "value" in the
 * program name RADIX tree. It basically contains another RADIX for
 * the per-program patterns.
 */
typedef struct _LogDBProgram
{
  RNode *rules;
} LogDBProgram;

typedef struct _LogPatternDatabase
{
  RNode *programs;
  gchar *version;
  gchar *pub_date;
} LogPatternDatabase;

LogDBResult *log_pattern_database_lookup(LogPatternDatabase *self, LogMessage *msg, GSList **dbg_list);
gboolean log_pattern_database_load(LogPatternDatabase *self, const gchar *config, GList **examples);
void log_pattern_database_free(LogPatternDatabase *self);

void log_pattern_example_free(gpointer s);

#endif
