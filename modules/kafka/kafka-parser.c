/*
 * Copyright (c) 2014 Pierre-Yves Ritschard <pyr@spootnik.org>
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Balazs Scheidler
 *
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
#include "kafka-grammar.h"

extern int kafka_debug;

int kafka_parse(CfgLexer *lexer, LogDriver **instance, gpointer arg);

static CfgLexerKeyword kafka_keywords[] =
{
  { "kafka",          KW_KAFKA },
  { "kafka_c",        KW_KAFKA_C },   /* compatibility with old incubator naming */

  { "topic",          KW_TOPIC },
  { "fallback_topic", KW_FALLBACK_TOPIC},
  { "partition",      KW_PARTITION},
  { "strategy_hint",  KW_STRATEGY_HINT },

  /* https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md */
  { "config",         KW_CONFIG },
  { "kafka_logging",  KV_KAFKA_LOGGING },
  { "flush_timeout_on_shutdown", KW_FLUSH_TIMEOUT_ON_SHUTDOWN },
  { "flush_timeout_on_reload",   KW_FLUSH_TIMEOUT_ON_RELOAD },

  { "key",            KW_KEY },
  { "message",        KW_MESSAGE },
  { "sync_send",      KW_SYNC_SEND},
  { "bootstrap_servers", KW_BOOTSTRAP_SERVERS },
  { "poll_timeout",   KW_POLL_TIMEOUT },
  { "single_worker_queue", KW_SINGLE_WORKER_QUEUE },
  { "log_fetch_queue_full_delay", KW_LOG_FETCH_QUEUE_FULL_DELAY },
  { "state_update_timeout",   KW_STATE_UPDATE_TIMEOUT },
  { "persist_store",  KW_PERSIST_STORE },
  { "store_metadata",   KW_STORE_METADATA },

  { NULL }
};

CfgParser kafka_parser =
{
#if SYSLOG_NG_ENABLE_DEBUG
  .debug_flag = &kafka_debug,
#endif
  .name = "kafka",
  .keywords = kafka_keywords,
  .parse = (gint (*)(CfgLexer *, gpointer *, gpointer)) kafka_parse,
  .cleanup = (void (*)(gpointer)) log_pipe_unref,
};

CFG_PARSER_IMPLEMENT_LEXER_BINDING(kafka_, KAFKA_, LogDriver **)
