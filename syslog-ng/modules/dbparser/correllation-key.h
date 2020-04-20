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
#ifndef PATTERNDB_CORRELLATION_KEY_H_INCLUDED
#define PATTERNDB_CORRELLATION_KEY_H_INCLUDED

#include "syslog-ng.h"

/* rule context scope */
typedef enum
{
  /* correllation happens globally, e.g. log messages even on different hosts are considered */
  RCS_GLOBAL,
  /* correllation happens inside the same host only, e.g. messages from other hosts are not considered */
  RCS_HOST,
  /* correllation happens for the same program only, e.g. messages from other programs are not considered */
  RCS_PROGRAM,
  /* correllation happens for the same process only, e.g. messages from a different program/pid are not considered */
  RCS_PROCESS,
} CorrellationScope;

gint correllation_key_lookup_scope(const gchar *scope);

/* Our state hash contains a mixed set of values, they are either
 * correllation contexts or the state entry required by rate limiting.
 */
typedef struct _CorrellationKey
{
  const gchar *host;
  const gchar *program;
  const gchar *pid;
  gchar *session_id;

  /* we use guint8 to limit the size of this structure, we can have 10s of
   * thousands of this structure present in memory */
  guint8 /* CorrellationScope */ scope;
} CorrellationKey;


guint correllation_key_hash(gconstpointer k);
gboolean correllation_key_equal(gconstpointer k1, gconstpointer k2);
void correllation_key_init(CorrellationKey *self, CorrellationScope scope, LogMessage *msg, gchar *session_id);

#endif
