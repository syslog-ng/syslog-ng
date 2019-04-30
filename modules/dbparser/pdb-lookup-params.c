/*
 * Copyright (c) 2019 Balabit
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

#include "pdb-lookup-params.h"

static void
_set_program_template(PDBLookupParams *lookup, LogTemplate *program_template)
{
  if (program_template)
    {
      lookup->program_handle = LM_V_NONE;
      lookup->program_template = program_template;
    }
  else
    {
      lookup->program_handle = LM_V_PROGRAM;
    }
};

void
pdb_lookup_params_init(PDBLookupParams *lookup, LogMessage *msg, LogTemplate *program_template)
{
  lookup->msg = msg;
  lookup->message_handle = LM_V_MESSAGE;
  lookup->message_len = 0;

  _set_program_template(lookup, program_template);
}
