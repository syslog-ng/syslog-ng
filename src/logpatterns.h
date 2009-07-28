/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
  guint ref_cnt;
} LogDBResult;


typedef struct _LogPatternDatabase
{
  RNode *programs;
  gchar *version;
  gchar *pub_date;
} LogPatternDatabase;

LogDBResult *log_pattern_database_lookup(LogPatternDatabase *self, LogMessage *msg);
gboolean log_pattern_database_load(LogPatternDatabase *self, const gchar *config);
void log_pattern_database_free(LogPatternDatabase *self);


#endif
