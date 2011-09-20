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

#include "affile.h"
#include "cfg-parser.h"
#include "affile-grammar.h"

extern int affile_debug;

int affile_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword affile_keywords[] = {
  { "file",               KW_FILE },
  { "fifo",               KW_PIPE },
  { "pipe",               KW_PIPE },

  { "fsync",              KW_FSYNC },
  { "remove_if_older",    KW_OVERWRITE_IF_OLDER, 0, KWS_OBSOLETE, "overwrite_if_older" },
  { "overwrite_if_older", KW_OVERWRITE_IF_OLDER },
  { "follow_freq",        KW_FOLLOW_FREQ,  },
  { "recursive",          KW_RECURSIVE },

  { NULL }
};

CfgParser affile_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &affile_debug,
#endif
  .name = "affile",
  .keywords = affile_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) affile_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(affile_, LogDriver **)
