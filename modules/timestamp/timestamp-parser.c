/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Vincent Bernat <Vincent.Bernat@exoscale.ch>
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

#include "timestamp-parser.h"
#include "timestamp-grammar.h"
#include "timestamp-parser.h"
#include "cfg-parser.h"
#include "logpipe.h"

extern int timestamp_debug;

static CfgLexerKeyword timestamp_keywords[] =
{
  { "date_parser",   KW_DATE_PARSER },
  { "time_stamp",    KW_TIME_STAMP },
  { "fix_time_zone", KW_FIX_TIME_ZONE },
  { "set_time_zone", KW_SET_TIME_ZONE },
  { "guess_time_zone", KW_GUESS_TIME_ZONE },
  { NULL }
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(timestamp_, gpointer *);

CfgParser timestamp_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &timestamp_debug,
#endif
  .name = "timestamp",
  .keywords = timestamp_keywords,
  .parse = (int (*)(CfgLexer *, gpointer *, gpointer)) timestamp_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};
