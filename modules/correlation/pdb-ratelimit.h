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

#ifndef PATTERNDB_PDB_RATELIMIT_H_INCLUDED
#define PATTERNDB_PDB_RATELIMIT_H_INCLUDED

#include "correlation-key.h"

/* This class encapsulates a rate-limit state stored in
   db->state. */
typedef struct _PDBRateLimit
{
  /* key in the hashtable. NOTE: host/program/pid/session_id are allocated, thus they need to be freed when the structure is freed. */
  CorrelationKey key;
  gint buckets;
  guint64 last_check;
} PDBRateLimit;

PDBRateLimit *pdb_rate_limit_new(CorrelationKey *key);
void pdb_rate_limit_free(PDBRateLimit *self);

#endif
