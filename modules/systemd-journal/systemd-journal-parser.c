/*
 * Copyright (c) 2010-2014 Balabit
 * Copyright (c) 2010-2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "systemd-journal.h"
#include "cfg-parser.h"
#include "systemd-journal-grammar.h"

extern int systemd_journal_debug;

int systemd_journal_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword systemd_journal_keywords[] =
{
  { "systemd_journal",            KW_SYSTEMD_JOURNAL },
  { "prefix",                     KW_PREFIX },
  { "max_field_size",             KW_MAX_FIELD_SIZE },
  { "namespace",                  KW_NAMESPACE },
  { NULL }
};

CfgParser systemd_journal_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &systemd_journal_debug,
#endif
  .name = "systemd-journal",
  .keywords = systemd_journal_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer arg)) systemd_journal_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(systemd_journal_, SYSTEMD_JOURNAL_, LogDriver **)
