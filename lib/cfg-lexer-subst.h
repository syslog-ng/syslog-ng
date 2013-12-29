/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#ifndef CFG_LEXER_SUBST_H_INCLUDED
#define CFG_LEXER_SUBST_H_INCLUDED 1

#include "syslog-ng.h"
#include "cfg-args.h"

typedef struct _CfgLexerSubst CfgLexerSubst;

gchar *cfg_lexer_subst_invoke(CfgLexerSubst *self, gchar *input, gssize input_len, gsize *output_length, GError **error);

CfgLexerSubst *cfg_lexer_subst_new(CfgArgs *globals, CfgArgs *defs, CfgArgs *args);
void cfg_lexer_subst_free(CfgLexerSubst *self);


#endif
