/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "driver.h"
#include "cfg-parser.h"
#include "affile-grammar.h"

extern int affile_debug;

int affile_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword affile_keywords[] =
{
  { "file",               KW_FILE },
  { "fifo",               KW_PIPE },
  { "pipe",               KW_PIPE },
  { "stdin",              KW_STDIN },

  { "wildcard_file",      KW_WILDCARD_FILE },
  { "base_dir",           KW_BASE_DIR },
  { "filename_pattern",   KW_FILENAME_PATTERN },
  { "recursive",          KW_RECURSIVE },
  { "max_files",          KW_MAX_FILES },
  { "monitor_method",     KW_MONITOR_METHOD },
  { "force_directory_polling", KW_FORCE_DIRECTORY_POLLING, KWS_OBSOLETE, "Use wildcard-file(monitor-method())" },

  { "fsync",              KW_FSYNC },
  { "remove_if_older",    KW_OVERWRITE_IF_OLDER, KWS_OBSOLETE, "overwrite_if_older" },
  { "overwrite_if_older", KW_OVERWRITE_IF_OLDER },
  { "follow_freq",        KW_FOLLOW_FREQ },
  { "multi_line_mode",    KW_MULTI_LINE_MODE  },
  { "multi_line_prefix",  KW_MULTI_LINE_PREFIX },
  { "multi_line_garbage", KW_MULTI_LINE_GARBAGE },
  { "multi_line_suffix",  KW_MULTI_LINE_GARBAGE },
  { "multi_line_timeout", KW_MULTI_LINE_TIMEOUT },
  { "time_reap",          KW_TIME_REAP },
  { NULL }
};

CfgParser affile_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &affile_debug,
#endif
  .name = "affile",
  .keywords = affile_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) affile_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(affile_, LogDriver **)
