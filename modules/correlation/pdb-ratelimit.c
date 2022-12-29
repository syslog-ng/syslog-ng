/*
 * Copyright (c) 2002-2017 Balabit
 * Copyright (c) 1998-2017 Balázs Scheidler
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
#include "pdb-ratelimit.h"
#include <string.h>

/***************************************************************************
 * PDBRateLimit
 ***************************************************************************/

PDBRateLimit *
pdb_rate_limit_new(CorrelationKey *key)
{
  PDBRateLimit *self = g_new0(PDBRateLimit, 1);

  memcpy(&self->key, key, sizeof(*key));
  if (self->key.pid)
    self->key.pid = g_strdup(self->key.pid);
  if (self->key.program)
    self->key.program = g_strdup(self->key.program);
  if (self->key.host)
    self->key.host = g_strdup(self->key.host);
  return self;
}

void
pdb_rate_limit_free(PDBRateLimit *self)
{
  if (self->key.host)
    g_free((gchar *) self->key.host);
  if (self->key.program)
    g_free((gchar *) self->key.program);
  if (self->key.pid)
    g_free((gchar *) self->key.pid);
  g_free(self->key.session_id);
  g_free(self);
}
