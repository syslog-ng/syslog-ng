/*
 * Copyright (c) 2022 Shikhar Vashistha
 * Copyright (c) 2022 László Várady
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
#include "file-rotation-grammar.h"

extern int file_rotation_debug;

int file_rotation_parse(CfgLexer *lexer, LogDriverPlugin **instance, gpointer arg);

static CfgLexerKeyword file_rotation_keywords[] =
{
  { "file_rotation", KW_FILE_ROTATION },
  { "size", KW_SIZE },
  { "interval", KW_INTERVAL },
  { NULL }
};

CfgParser file_rotation_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &file_rotation_debug,
#endif
  .name = "file-rotation",
  .keywords = file_rotation_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) file_rotation_parse,
  .cleanup = (void (*)(gpointer)) log_driver_plugin_free,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(file_rotation_, FILE_ROTATION_, LogDriverPlugin **)
