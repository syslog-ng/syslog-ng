/*
 * Copyright (c) 2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2013 Tihamer Petrovics <tihameri@gmail.com>
 * Copyright (c) 2014 Pierre-Yves Ritschard <pyr@spootnik.org>
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

#include <librdkafka/rdkafka.h>
#include <stdlib.h>
#include <zlib.h>

#include "kafka-destination.h"
#include "kafka-parser.h"
#include "plugin.h"
#include "messages.h"
#include "stats/stats.h"
#include "logqueue.h"
#include "driver.h"
#include "plugin-types.h"
#include "logthrdest/logthrdestdrv.h"
#include "seqnum.h"

/*
 * This module draws from the redis module and provides an Apache Kafka
 * output. Please refer to http://kafka.apache.org for details on the
 * kafka distributed queue.
 *
 * This module accepts the following options:
 * - _properties_, mandatory. Sets global properties, you will need at least
 *   "metadata.broker.list" set
 * - _topic_, mandatory. Expects a named topic and optional associated
 *   metadata such as the number of partitions
 * - _payload_, mandatory. A template to describe payload content
 * - _partition_, optional. Describes the partitioning method for the topic.
 *   a random partition is assigned by default. Accepts the following arguments:
 *   "random" for random partitions any other string to use the checksum of a
 *   message template.
 */

#ifndef SCS_KAFKA
#define SCS_KAFKA 0
#endif

GList *last_property = NULL;

#define KAFKA_FLAG_NONE 0
#define KAFKA_FLAG_SYNC 0x0001

typedef struct
{
  LogThreadedDestDriver super;

  gchar *topic_name;
  gchar *key_str;
  LogTemplate *field;

  LogTemplateOptions template_options;
  LogTemplateOptions field_template_options;

  GString *payload_str;
  LogTemplate *payload;

  gint32 flags;
  gint32 seq_num;
  rd_kafka_topic_t *topic;
  rd_kafka_t *kafka;
  enum
  {
    PARTITION_RANDOM = 0,
    PARTITION_FIELD = 1
  } partition_type;
} KafkaDriver;

void
kafka_log(const rd_kafka_t *rkt, int level,
          const char *fac, const char *msg)
{
  msg_event_suppress_recursions_and_send(
    msg_event_create(level,
                     "Kafka internal message",
                     evt_tag_str("msg", msg),
                     NULL ));
}

void
kafka_property_free(void *p)
{
  struct kafka_property *kp = p;

  g_free(kp->key);
  g_free(kp->val);
  g_free(kp);
}

int32_t kafka_partition(const rd_kafka_topic_t *rkt,
                        const void *keydata,
                        size_t keylen,
                        int32_t partition_cnt,
                        void *rktp,
                        void *msgp)
{
  u_int32_t key = *((u_int32_t *)keydata);
  u_int32_t target = key % partition_cnt;
  int32_t i = partition_cnt;

  while (--i > 0 && !rd_kafka_topic_partition_available(rkt, target)) {
    target = (target + 1) % partition_cnt;
  }
  return target;
}

static void
kafka_worker_sync_produce_dr_cb(rd_kafka_t *rk,
                                void *payload, size_t len,
                                rd_kafka_resp_err_t err,
                                void *opaque, void *msg_opaque)
{
  /* When done, just copy error code */
  rd_kafka_resp_err_t *errp = (rd_kafka_resp_err_t *)msg_opaque;
  *errp = err;
}

/*
 * Configuration
 */

void
kafka_dd_set_props(LogDriver *d, GList *props)
{
  KafkaDriver *self = (KafkaDriver *)d;
  GList *list;
  struct kafka_property *kp;
  rd_kafka_conf_t *conf;
  char errbuf[1024];

  bzero(errbuf, sizeof(errbuf));

  conf = rd_kafka_conf_new();
  for (list = g_list_first(props); list != NULL; list = g_list_next(list))
    {
      kp = list->data;
      msg_debug("setting kafka property",
                evt_tag_str("key", kp->key),
                evt_tag_str("val", kp->val),
                NULL);
      rd_kafka_conf_set(conf, kp->key, kp->val,
                        errbuf, sizeof(errbuf));
    }
#ifdef HAVE_LIBRDKAFKA_LOG_CB
  rd_kafka_conf_set_log_cb(conf, kafka_log);
#endif
  if (self->flags & KAFKA_FLAG_SYNC)
    {
      msg_info("synchronous insertion into kafka, "
               "lower the value of queue.buffering.max.ms to increase performance",
               evt_tag_str("driver", self->super.super.super.id),
               NULL);
      rd_kafka_conf_set_dr_cb(conf, kafka_worker_sync_produce_dr_cb);
    }

  self->kafka = rd_kafka_new(RD_KAFKA_PRODUCER, conf,
                             errbuf, sizeof(errbuf));
#ifdef HAVE_LIBRDKAFKA_LOGGER
  if (self->kafka != NULL)
    {
      rd_kafka_set_logger(self->kafka, kafka_log);
    }
#endif
}

void
kafka_dd_set_partition_random(LogDriver *d)
{
  KafkaDriver *self = (KafkaDriver *)d;

  self->partition_type = PARTITION_RANDOM;
}

void
kafka_dd_set_partition_field(LogDriver *d, LogTemplate *field)
{
  KafkaDriver *self = (KafkaDriver *)d;

  self->partition_type = PARTITION_FIELD;
  log_template_unref(self->field);
  self->field = log_template_ref(field);
}

void
kafka_dd_set_flag_sync(LogDriver *d)
{
  KafkaDriver *self = (KafkaDriver *)d;
  if (self->kafka != NULL)
    {
      msg_error("kafka flags must be set before kafka properties", NULL);
      return;
    }
  self->flags |= KAFKA_FLAG_SYNC;
}

void
kafka_dd_set_topic(LogDriver *d, const gchar *topic, GList *props)
{
  KafkaDriver *self = (KafkaDriver *)d;
  GList *list;
  struct kafka_property *kp;
  rd_kafka_topic_conf_t *topic_conf;
  char errbuf[1024];

  if (self->kafka == NULL)
    {
      msg_error("kafka topic must be set after kafka properties", NULL);
      return;
    }

  topic_conf = rd_kafka_topic_conf_new();

  for (list = g_list_first(props); list != NULL; list = g_list_next(list))
    {
      kp = list->data;
      msg_debug("setting kafka topic property",
                evt_tag_str("key", kp->key),
                evt_tag_str("val", kp->val),
                NULL);
      rd_kafka_topic_conf_set(topic_conf, kp->key, kp->val,
                              errbuf, sizeof(errbuf));
    }

  rd_kafka_topic_conf_set_partitioner_cb(topic_conf, kafka_partition);
  rd_kafka_topic_conf_set_opaque(topic_conf, self);
  self->topic_name = g_strdup(topic);
  self->topic = rd_kafka_topic_new(self->kafka, topic, topic_conf);
}

void
kafka_dd_set_payload(LogDriver *d, LogTemplate *payload)
{
  KafkaDriver *self = (KafkaDriver *)d;

  log_template_unref(self->payload);
  self->payload = log_template_ref(payload);
}

/*
 * Utilities
 */

LogTemplateOptions *
kafka_dd_get_template_options(LogDriver *d)
{
  KafkaDriver *self = (KafkaDriver *)d;

  return &self->template_options;
}

static const gchar *
kafka_dd_format_stats_instance(LogThreadedDestDriver *d)
{
  KafkaDriver *self = (KafkaDriver *)d;
  static gchar persist_name[1024];

  if (d->super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "kafka,%s", d->super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "kafka,%s", self->topic_name);
  return persist_name;
}

static const gchar *
kafka_dd_format_persist_name(const LogPipe *d)
{
  const KafkaDriver *self = (const KafkaDriver *)d;
  static gchar persist_name[1024];

  if (d->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "kafka.%s", d->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "kafka(%s)", self->topic_name);
  return persist_name;
}

static u_int32_t
kafka_calculate_partition_key(KafkaDriver *self, LogMessage *msg)
{
  u_int32_t key;
  GString *field = g_string_sized_new(1024);

  log_template_format(self->field, msg, &self->field_template_options,
			LTZ_SEND, self->seq_num, NULL, field);

  msg_debug("Kafka dynamic key",
	    evt_tag_str("key", field->str),
	    evt_tag_str("driver", self->super.super.super.id),
	    NULL);

  key = crc32(0L, Z_NULL, 0);
  key = crc32(key, (const u_char *)field->str, field->len);

  msg_debug("Kafka field crc32",
            evt_tag_str("payload", field->str),
	    evt_tag_int("length", field->len),
	    evt_tag_int("crc32", key),
	    evt_tag_str("driver", self->super.super.super.id),
	    NULL);

  g_string_free(field, TRUE);
  return key;
}

/*
 * Worker thread
 */

static LogThreadedResult
kafka_worker_insert(LogThreadedDestDriver *s, LogMessage *msg)
{
  KafkaDriver *self = (KafkaDriver *)s;
  gboolean success;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  GString *field;
  u_int32_t key;

  log_template_format(self->payload, msg, &self->template_options, LTZ_SEND,
                      self->seq_num, NULL, self->payload_str);

  switch (self->partition_type)
    {
    case PARTITION_RANDOM:
      key =  rand();
      break;
    case PARTITION_FIELD:
      key = kafka_calculate_partition_key(self, msg);
      break;
    default:
      key = 0;
    }

#define KAFKA_INITIAL_ERROR_CODE -12345
  rd_kafka_resp_err_t err = KAFKA_INITIAL_ERROR_CODE;
  if (rd_kafka_produce(self->topic,
                       RD_KAFKA_PARTITION_UA,
                       RD_KAFKA_MSG_F_COPY,
                       self->payload_str->str,
                       self->payload_str->len,
                       &key, sizeof(key),
                       &err) == -1)
    {
      msg_error("Failed to add message to Kafka topic!",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("topic", self->topic_name),
                evt_tag_str("error", rd_kafka_err2str(rd_kafka_errno2err(errno))),
                NULL);
      return LTR_ERROR;
    }

  msg_debug("Kafka produce done",
            evt_tag_str("driver", self->super.super.super.id),
            NULL);

  /* Wait for completion. */
  if (self->flags & KAFKA_FLAG_SYNC)
    {
      while (err == KAFKA_INITIAL_ERROR_CODE)
        {
          rd_kafka_poll(self->kafka, 5000);
          if (err == KAFKA_INITIAL_ERROR_CODE)
            {
              msg_debug("Kafka producer is freezed",
                        evt_tag_str("driver", self->super.super.super.id),
                        evt_tag_str("topic", self->topic_name),
                        NULL);
            }
        }
      if (err != RD_KAFKA_RESP_ERR_NO_ERROR)
        {
          msg_error("Failed to add message to Kafka topic!",
                    evt_tag_str("driver", self->super.super.super.id),
                    evt_tag_str("topic", self->topic_name),
                    evt_tag_str("error", rd_kafka_err2str(err)),
                    NULL);
          return LTR_ERROR;
        }
    }

  msg_debug("Kafka event sent",
            evt_tag_str("driver", self->super.super.super.id),
            evt_tag_str("topic", self->topic_name),
            evt_tag_str("payload", self->payload_str->str),
            NULL);

  return LTR_SUCCESS;
}

static void
kafka_worker_thread_init(LogThreadedDestDriver *d)
{
  KafkaDriver *self = (KafkaDriver *)d;

  msg_debug("Worker thread started",
            evt_tag_str("driver", self->super.super.super.id),
            NULL);

  self->payload_str = g_string_sized_new(1024);

}

static void
kafka_worker_thread_deinit(LogThreadedDestDriver *d)
{
  KafkaDriver *self = (KafkaDriver *)d;

  g_string_free(self->payload_str, TRUE);
}

/*
 * Main thread
 */

static gboolean
kafka_dd_init(LogPipe *s)
{
  KafkaDriver *self = (KafkaDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_threaded_dest_driver_init_method(s))
    return FALSE;

  log_template_options_init(&self->template_options, cfg);
  log_template_options_init(&self->field_template_options, cfg);

  msg_verbose("Initializing Kafka destination",
              evt_tag_str("driver", self->super.super.super.id),
              NULL);

  if (self->topic == NULL)
    {
      msg_error("Kafka producer is not set up properly, topic name is missing",
		evt_tag_str("driver", self->super.super.super.id),
                NULL);
      return FALSE;
    }

  if (self->payload == NULL)
    {
      self->payload = log_template_new(cfg, "default_kafka_template");
      log_template_compile(self->payload, "$MESSAGE", NULL);
    }

  if (self->kafka == NULL)
    {
      msg_error("Kafka producer is not set up properly, perhaps metadata.broker.list property is missing?",
		evt_tag_str("driver", self->super.super.super.id),
                NULL);
      return FALSE;
    }

  return log_threaded_dest_driver_start_workers(&self->super);
}

static void
kafka_dd_free(LogPipe *d)
{
  KafkaDriver *self = (KafkaDriver *)d;

  log_template_options_destroy(&self->template_options);
  log_template_options_destroy(&self->field_template_options);

  log_template_unref(self->payload);
  if (self->topic)
    rd_kafka_topic_destroy(self->topic);
  if (self->kafka)
    rd_kafka_destroy(self->kafka);
  if (self->topic_name)
    g_free(self->topic_name);
  log_threaded_dest_driver_free(d);
}

/*
 * Plugin glue.
 */

LogDriver *
kafka_dd_new(GlobalConfig *cfg)
{
  KafkaDriver *self = g_new0(KafkaDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);
  self->super.super.super.super.init = kafka_dd_init;
  self->super.super.super.super.free_fn = kafka_dd_free;
  self->super.super.super.super.generate_persist_name = kafka_dd_format_persist_name;

  self->super.worker.thread_init = kafka_worker_thread_init;
  self->super.worker.thread_deinit = kafka_worker_thread_deinit;
  self->super.worker.insert = kafka_worker_insert;

  self->super.format_stats_instance = kafka_dd_format_stats_instance;
  self->super.stats_source = SCS_KAFKA;

  self->flags = KAFKA_FLAG_NONE;

  init_sequence_number(&self->seq_num);
  log_template_options_defaults(&self->template_options);
  log_template_options_defaults(&self->field_template_options);

  return (LogDriver *)self;
}
