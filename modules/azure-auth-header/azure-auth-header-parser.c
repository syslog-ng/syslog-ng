/*
 * Copyright (c) 2020 One Identity
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

#include "cfg-parser.h"

#include "azure-auth-header.h"
#include "azure-auth-header-grammar.h"

extern int azure_auth_header_debug;

int azure_auth_header_parse(CfgLexer *lexer, LogDriverPlugin **instance, gpointer arg);

static CfgLexerKeyword azure_auth_header_keywords[] =
{
  { "azure_auth_header", KW_AZURE_AUTH },
  { "workspace_id",      KW_WORKSPACE_ID },
  { "secret",            KW_SECRET },
  { "method",            KW_METHOD },
  { "path",              KW_PATH },
  { "content_type",      KW_CONTENT_TYPE },
  { NULL }
};

CfgParser azure_auth_header_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &azure_auth_header_debug,
#endif
  .name = "azure_auth_header",
  .keywords = azure_auth_header_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) azure_auth_header_parse,
  .cleanup = (void (*)(gpointer)) log_driver_plugin_free,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(azure_auth_header_, LogDriverPlugin **)
