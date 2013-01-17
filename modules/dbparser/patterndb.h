/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#ifndef PATTERNDB_H_INCLUDED
#define PATTERNDB_H_INCLUDED

#include "radix.h"
#include "templates.h"
#include "timerwheel.h"
#include "filter.h"

typedef struct _PatternDB PatternDB;

typedef struct _PDBInput
{
  LogMessage *msg;
  NVHandle program_handle;
  NVHandle message_handle;
  const gchar *message_string;
  gssize message_len;
} PDBInput;

#define PDB_INPUT_DEFAULT(msg) { (msg), LM_V_PROGRAM, LM_V_MESSAGE, NULL, 0 }
#define PDB_INPUT_MESSAGE(msg) ({ PDBInput __pdb_input = { (msg), LM_V_PROGRAM, LM_V_MESSAGE, NULL, 0 }; &__pdb_input; })

typedef void (*PatternDBEmitFunc)(LogMessage *msg, gboolean synthetic, gpointer user_data);
void pattern_db_set_emit_func(PatternDB *self, PatternDBEmitFunc emit_func, gpointer emit_data);

const gchar *pattern_db_get_ruleset_version(PatternDB *self);
const gchar *pattern_db_get_ruleset_pub_date(PatternDB *self);
gboolean pattern_db_reload_ruleset(PatternDB *self, GlobalConfig *cfg, const gchar *pdb_file);

void pattern_db_timer_tick(PatternDB *self);
gboolean pattern_db_process(PatternDB *self, PDBInput *input);
void pattern_db_expire_state(PatternDB *self);
void pattern_db_forget_state(PatternDB *self);

PatternDB *pattern_db_new(void);
void pattern_db_free(PatternDB *self);

void pattern_db_global_init(void);

#endif
