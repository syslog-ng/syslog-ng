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

#include <librdkafka/rdkafka.h>

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

static void
_logger_glue(const rd_kafka_t *rk, int level, const char *fac, const char *buf)
{
  msg_error(buf);
}

struct KafkaSourceDriver
{
  LogThreadedFetcherDriver super;

  rd_kafka_conf_t *conf;
  rd_kafka_topic_conf_t *topic_conf;
  rd_kafka_t *rk;
  rd_kafka_topic_partition_list_t *topics;

  gboolean should_exit;

  gchar *brokers;
  gchar *topic;
};

static gboolean
_connect(LogThreadedFetcherDriver *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;

  gchar errstr[255];
  if (!(self->rk = rd_kafka_new(RD_KAFKA_CONSUMER, self->conf, errstr, sizeof(errstr))))
    {
      msg_error("Kafka new error", evt_tag_str("error",errstr));
      return FALSE;
    }

  const gint number_of_brokers = rd_kafka_brokers_add(self->rk, self->brokers);
  if (number_of_brokers == 0)
    {
      msg_error("Kafka no valid broker specified", evt_tag_str("brokers",self->brokers));
      return FALSE;
    }

  self->topics = rd_kafka_topic_partition_list_new(1);
  rd_kafka_topic_partition_list_add(self->topics, self->topic, 0);

  rd_kafka_resp_err_t err;
  if ((err = rd_kafka_assign(self->rk, self->topics)))
    {
      msg_error("Kafka failed to assing partitions", evt_tag_str("error",rd_kafka_err2str(err)));
      return FALSE;
    }

  rd_kafka_poll_set_consumer(self->rk);

  return TRUE;
}

static void
_disconnect(LogThreadedFetcherDriver *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *)s;

  rd_kafka_resp_err_t err = rd_kafka_consumer_close(self->rk);
  if (err)
    {
      msg_error("Failed to close consumer", evt_tag_str("error",rd_kafka_err2str(err)));
    }

  rd_kafka_topic_partition_list_destroy(self->topics);

  rd_kafka_destroy(self->rk);
}

static gboolean
_skip_error(rd_kafka_message_t *message)
{
  if (!message)
    return TRUE;

  if (message->err == RD_KAFKA_RESP_ERR__PARTITION_EOF)
    return TRUE;

  return FALSE;
}

/* runs in a dedicated thread */
static LogThreadedFetchResult
_fetch(LogThreadedFetcherDriver *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) s;

  LogThreadedFetchResult result;

  if (!self->rk)
    goto no_message;

  rd_kafka_message_t *rkmessage;
  while (!self->should_exit)
    {
      rkmessage = rd_kafka_consumer_poll(self->rk, 1000);
      if (!_skip_error(rkmessage))
        break;
    }

  if (self->should_exit)
    goto no_message;

  if (rkmessage->err)
    goto error;

  LogMessage *msg = log_msg_new_empty();

  log_msg_set_value(msg, LM_V_MESSAGE, rkmessage->payload, rkmessage->len);

  rd_kafka_message_destroy(rkmessage);
  result.result = THREADED_FETCH_SUCCESS;
  result.msg = msg;
  return result;

error:
  msg_error("error",
            evt_tag_str("err",rd_kafka_message_errstr(rkmessage))
           );
  rd_kafka_message_destroy(rkmessage);
no_message:
  result.result = THREADED_FETCH_NOT_CONNECTED;
  result.msg = NULL;
  return result;
}

static void
_request_exit(LogThreadedFetcherDriver *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) s;
  self->should_exit = TRUE;
}

static const gchar *
_format_stats_instance(LogThreadedSourceDriver *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) s;
  static gchar persist_name[1024];

  if (s->super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "kafka-source,%s", s->super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "kafka-source,%s", self->brokers);

  return persist_name;
}

static gboolean
_check_options(KafkaSourceDriver *self)
{
  if (!self->brokers)
    {
      msg_error("The brokers() option for kafka() is mandatory", log_pipe_location_tag(&self->super.super.super.super.super));
      return FALSE;
    }
  if (!self->topic)
    {
      msg_error("The topic() option for kafka() is mandatory", log_pipe_location_tag(&self->super.super.super.super.super));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_init(LogPipe *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) s;

  self->should_exit = FALSE;

  if (!_check_options(self))
    return FALSE;

  self->conf = rd_kafka_conf_new();
  if (!self->conf)
    return FALSE;

  rd_kafka_conf_set_log_cb(self->conf, _logger_glue);

  gchar errstr[255];
  const gchar *group = _format_stats_instance(&self->super.super);
  if (rd_kafka_conf_set(self->conf, "group.id", group, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
      msg_error("Kafka config set error", evt_tag_str("error",errstr));
      return FALSE;
    }

  self->topic_conf = rd_kafka_topic_conf_new();

  rd_kafka_conf_set_default_topic_conf(self->conf, self->topic_conf);

  if (rd_kafka_topic_conf_set(self->topic_conf, "offset.store.method", "broker", errstr,
                              sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
      msg_error("Kafka config set error", evt_tag_str("error",errstr));
      return FALSE;
    }

  return log_threaded_fetcher_driver_init_method(s);
}

static void
_free(LogPipe *s)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) s;

  g_free(self->brokers);
  g_free(self->topic);

  log_threaded_fetcher_driver_free_method(s);
}

void
kafka_sd_set_brokers(LogDriver *s, const gchar *brokers)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) s;

  g_free(self->brokers);
  self->brokers = g_strdup(brokers);
}

void
kafka_sd_set_topic(LogDriver *s, const gchar *topic)
{
  KafkaSourceDriver *self = (KafkaSourceDriver *) s;

  g_free(self->topic);
  self->topic = g_strdup(topic);
}

LogDriver *
kafka_sd_new(GlobalConfig *cfg)
{
  KafkaSourceDriver *self = g_new0(KafkaSourceDriver, 1);
  log_threaded_fetcher_driver_init_instance(&self->super, cfg);

  self->super.connect = _connect;
  self->super.disconnect = _disconnect;
  self->super.fetch = _fetch;
  self->super.request_exit = _request_exit;

  self->super.super.super.super.super.init = _init;
  self->super.super.super.super.super.free_fn = _free;
  self->super.super.format_stats_instance = _format_stats_instance;

  return &self->super.super.super.super;
}
