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

#ifndef PATTERNDB_PDB_LOOKUP_PARAMS_H_INCLUDED
#define PATTERNDB_PDB_LOOKUP_PARAMS_H_INCLUDED

typedef struct _PDBLookupParams PDBLookupParams;
struct _PDBLookupParams
{
  LogMessage *msg;
  NVHandle program_handle;
  NVHandle message_handle;
  const gchar *message_string;
  gssize message_len;
};

static inline void
pdb_lookup_params_init(PDBLookupParams *lookup, LogMessage *msg)
{
  lookup->msg = msg;
  lookup->program_handle = LM_V_PROGRAM;
  lookup->message_handle = LM_V_MESSAGE;
  lookup->message_len = 0;
}

static inline void
pdb_lookup_params_override_message(PDBLookupParams *lookup, const gchar *message, gssize message_len)
{
  lookup->message_handle = LM_V_NONE;
  lookup->message_string = message;
  lookup->message_len = message_len;
}
#endif
