/*
 * Copyright (c) 2016 Marc Falzon
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
#include "curl-grammar.h"

extern int curl_debug;

int curl_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword curl_keywords[] = {
  { "curl",         KW_CURL },
  { "user",         KW_USER },
  { "password",     KW_PASSWORD },
  { "user_agent",   KW_USER_AGENT },
  { "url",          KW_URL },
  { "headers",      KW_HEADERS },
  { "body",         KW_BODY },
  { NULL }
};

CfgParser curl_parser =
{
#if ENABLE_DEBUG
  .debug_flag = &curl_debug,
#endif
  .name = "curl",
  .keywords = curl_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) curl_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(curl_, LogDriver **)
