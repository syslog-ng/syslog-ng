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
#ifndef PATTERNDB_CORRELATION_KEY_H_INCLUDED
#define PATTERNDB_CORRELATION_KEY_H_INCLUDED

#include "syslog-ng.h"

/* rule context scope */
typedef enum
{
  /* correlation happens globally, e.g. log messages even on different hosts are considered */
  RCS_GLOBAL,
  /* correlation happens inside the same host only, e.g. messages from other hosts are not considered */
  RCS_HOST,
  /* correlation happens for the same program only, e.g. messages from other programs are not considered */
  RCS_PROGRAM,
  /* correlation happens for the same process only, e.g. messages from a different program/pid are not considered */
  RCS_PROCESS,
} CorrelationScope;

gint correlation_key_lookup_scope(const gchar *scope);

/* Our state hash contains a mixed set of values, they are either
 * correlation contexts or the state entry required by rate limiting.
 */
typedef struct _CorrelationKey
{
  const gchar *host;
  const gchar *program;
  const gchar *pid;
  gchar *session_id;

  /* we use guint8 to limit the size of this structure, we can have 10s of
   * thousands of this structure present in memory */
  guint8 /* CorrelationScope */ scope;
} CorrelationKey;


guint correlation_key_hash(gconstpointer k);
gboolean correlation_key_equal(gconstpointer k1, gconstpointer k2);
void correlation_key_init(CorrelationKey *self, CorrelationScope scope, LogMessage *msg, gchar *session_id);

#endif
