/*
 * Copyright (c) 2021 One Identity
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
#include "mqtt-grammar.h"


extern int mqtt_destination_debug;

int mqtt_destination_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword mqtt_destination_keywords[] =
{
  { "mqtt", KW_MQTT },
  { "address", KW_ADDRESS },
  { "topic", KW_TOPIC },
  { "fallback_topic", KW_FALLBACK_TOPIC },
  { "keepalive", KW_KEEPALIVE },
  { "qos", KW_QOS },
  { "username", KW_USERNAME },
  { "password", KW_PASSWORD },
  { "http_proxy", KW_HTTP_PROXY },
  { NULL }
};

CfgParser mqtt_destination_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &mqtt_destination_debug,
#endif
  .name = "mqtt_destination",
  .keywords = mqtt_destination_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) mqtt_destination_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(mqtt_destination_, MQTT_DESTINATION_, LogDriver **)
