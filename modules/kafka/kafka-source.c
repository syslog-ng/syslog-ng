/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 Kokan <kokaipeter@gmail.com>
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

#include "kafka-source.h"
#include "logthrsource/logthrfetcherdrv.h"
#include "logmsg/logmsg.h"
#include "messages.h"

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

struct KafkaSourceDriver
{
  LogThreadedFetcherDriver super;

};

static gboolean
_connect(LogThreadedFetcherDriver *s)
{

  return TRUE;
}

static void
_disconnect(LogThreadedFetcherDriver *s)
{

}

/* runs in a dedicated thread */
static LogThreadedFetchResult
_fetch(LogThreadedFetcherDriver *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) s;

  LogMessage *msg = log_msg_new_empty();

  if (!msg)
    {
      LogThreadedFetchResult result = { THREADED_FETCH_NOT_CONNECTED, NULL };
      return result;
    }

  LogThreadedFetchResult result = { THREADED_FETCH_SUCCESS, msg };
  return result;
}

static gboolean
_init(LogPipe *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) s;

  return log_threaded_fetcher_driver_init_method(s);
}

static void
_free(LogPipe *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) s;

  log_threaded_fetcher_driver_free_method(s);
}

static const gchar *
_format_stats_instance(LogThreadedSourceDriver *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) s;
  static gchar persist_name[1024];

  if (s->super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "kafka-source,%s", s->super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "kafka-source");

  return persist_name;
}

LogDriver *
kafka_sd_new(GlobalConfig *cfg)
{
  KafkaSourceDriver *self = g_new0(KafkaSourceDriver, 1);
  log_threaded_fetcher_driver_init_instance(&self->super, cfg);

  self->super.connect = _connect;
  self->super.disconnect = _disconnect;
  self->super.fetch = _fetch;

  self->super.super.super.super.super.init = _init;
  self->super.super.super.super.super.free_fn = _free;
  self->super.super.format_stats_instance = _format_stats_instance;

  return &self->super.super.super.super;
}
