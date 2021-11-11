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


extern int mqtt_debug;

int mqtt_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword mqtt_keywords[] =
{
  { "mqtt", KW_MQTT },
  { "address", KW_ADDRESS },
  { "topic", KW_TOPIC },
  { "fallback_topic", KW_FALLBACK_TOPIC },
  { "keepalive", KW_KEEPALIVE },
  { "qos", KW_QOS },
  { "client_id", KW_CLIENT_ID },
  { "cleansession", KW_CLEANSESSION },
  { "username", KW_USERNAME },
  { "password", KW_PASSWORD },
  { "http_proxy", KW_HTTP_PROXY },
  { "tls", KW_TLS },
  { "ca_dir", KW_CA_DIR },
  { "ca_file", KW_CA_FILE },
  { "cert_file", KW_CERT_FILE },
  { "key_file", KW_KEY_FILE },
  { "cipher_suite", KW_CIPHER_SUITE },
  { "peer_verify", KW_PEER_VERIFY },
  { "use_system_cert_store", KW_USE_SYSTEM_CERT_STORE },
  { "ssl_version", KW_SSL_VERSION },
  { NULL }
};

CfgParser mqtt_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &mqtt_debug,
#endif
  .name = "mqtt",
  .keywords = mqtt_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) mqtt_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(mqtt_, MQTT_, LogDriver **)
