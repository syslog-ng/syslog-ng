/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
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

#include "afsnmpdest.h"
#include "cfg-parser.h"
#include "afsnmp-grammar.h"
#include "afsnmp-parser.h"

int afsnmp_parse(CfgLexer *lexer, void **instance, gpointer arg);

static CfgLexerKeyword afsnmp_keywords[] =
{
  { "snmp",              KW_SNMPDEST },
  { "version",           KW_VERSION },
  { "host",              KW_HOST },
  { "port",              KW_PORT },
  { "snmp_obj",          KW_SNMP_OBJ },
  { "trap_obj",          KW_TRAP_OBJ },
  { "community",         KW_COMMUNITY },
  { "engine_id",         KW_ENGINE_ID },
  { "auth_username",     KW_AUTH_USERNAME },
  { "auth_algorithm",    KW_AUTH_ALGORITHM },
  { "auth_password",     KW_AUTH_PASSWORD },
  { "enc_algorithm",     KW_ENC_ALGORITHM },
  { "enc_password",      KW_ENC_PASSWORD },
  { "transport",         KW_TRANSPORT },
  { "time_zone",         KW_LOCAL_TIME_ZONE },
  { "snmptrapd_parser",  KW_SNMPTRAPD_PARSER },
  { "prefix",            KW_PREFIX },
  { "set_message_macro", KW_SET_MESSAGE_MACRO },
  { NULL }
};

CfgParser afsnmp_parser =
{
  .name = "afsnmp",
  .keywords = afsnmp_keywords,
  .parse = (int (*)(CfgLexer *lexer, gpointer *instance, gpointer)) afsnmp_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(afsnmp_, AFSNMP_, void **)
