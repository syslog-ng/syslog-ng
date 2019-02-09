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


#include "kafka-dest-driver.h"
#include "kafka-props.h"
#include "kafka-dest-worker.h"

#include <librdkafka/rdkafka.h>
#include <stdlib.h>

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
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  kafka_property_list_free(self->props);
  self->props = props;
}

void
kafka_dd_set_partition_random(LogDriver *d)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  self->partition_type = PARTITION_RANDOM;
}

void
kafka_dd_set_partition_field(LogDriver *d, LogTemplate *field)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  self->partition_type = PARTITION_FIELD;
  log_template_unref(self->field);
  self->field = log_template_ref(field);
}

void
kafka_dd_set_flag_sync(LogDriver *d)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;
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
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  g_free(self->topic_name);
  kafka_property_list_free(self->topic_props);
  self->topic_name = g_strdup(topic);
  self->topic_props = props;
}

void
kafka_dd_set_payload(LogDriver *d, LogTemplate *payload)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  log_template_unref(self->payload);
  self->payload = log_template_ref(payload);
}

/*
 * Utilities
 */

LogTemplateOptions *
kafka_dd_get_template_options(LogDriver *d)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  return &self->template_options;
}

static const gchar *
kafka_dd_format_stats_instance(LogThreadedDestDriver *d)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;
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
  const KafkaDestDriver *self = (const KafkaDestDriver *)d;
  static gchar persist_name[1024];

  if (d->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "kafka.%s", d->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "kafka(%s)", self->topic_name);
  return persist_name;
}


/*
 * Main thread
 */

static rd_kafka_t *
_construct_client(KafkaDestDriver *self)
{
  GList *list;
  KafkaProperty *kp;
  rd_kafka_t *client;
  rd_kafka_conf_t *conf;
  char errbuf[1024];

  bzero(errbuf, sizeof(errbuf));

  conf = rd_kafka_conf_new();
  for (list = g_list_first(self->props); list != NULL; list = g_list_next(list))
    {
      kp = list->data;
      msg_debug("setting kafka property",
                evt_tag_str("key", kp->name),
                evt_tag_str("val", kp->value),
                NULL);
      rd_kafka_conf_set(conf, kp->name, kp->value,
                        errbuf, sizeof(errbuf));
    }
  rd_kafka_conf_set_log_cb(conf, kafka_log);
  if (self->flags & KAFKA_FLAG_SYNC)
    {
      msg_info("synchronous insertion into kafka, "
               "lower the value of queue.buffering.max.ms to increase performance",
               evt_tag_str("driver", self->super.super.super.id),
               NULL);
      rd_kafka_conf_set_dr_cb(conf, kafka_worker_sync_produce_dr_cb);
    }

  client = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errbuf, sizeof(errbuf));
  return client;
}

rd_kafka_topic_t *
_construct_topic(KafkaDestDriver *self)
{
  GList *list;
  KafkaProperty *kp;
  rd_kafka_topic_conf_t *topic_conf;
  char errbuf[1024];

  g_assert(self->kafka != NULL);

  topic_conf = rd_kafka_topic_conf_new();

  for (list = g_list_first(self->topic_props); list != NULL; list = g_list_next(list))
    {
      kp = list->data;
      msg_debug("setting kafka topic property",
                evt_tag_str("key", kp->name),
                evt_tag_str("val", kp->value),
                NULL);
      rd_kafka_topic_conf_set(topic_conf, kp->name, kp->value,
                              errbuf, sizeof(errbuf));
    }

  rd_kafka_topic_conf_set_partitioner_cb(topic_conf, kafka_partition);
  rd_kafka_topic_conf_set_opaque(topic_conf, self);
  return rd_kafka_topic_new(self->kafka, self->topic_name, topic_conf);
}

static LogThreadedDestWorker *
_construct_worker(LogThreadedDestDriver *s, gint worker_index)
{
  return kafka_dest_worker_new(s, worker_index);
}

static gboolean
kafka_dd_init(LogPipe *s)
{
  KafkaDestDriver *self = (KafkaDestDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_threaded_dest_driver_init_method(s))
    return FALSE;

  log_template_options_init(&self->template_options, cfg);

  self->kafka = _construct_client(self);
  if (self->kafka == NULL)
    {
      msg_error("Kafka producer is not set up properly, perhaps metadata.broker.list property is missing?",
		evt_tag_str("driver", self->super.super.super.id),
                NULL);
      return FALSE;
    }

  self->topic = _construct_topic(self);
  if (self->topic == NULL)
    {
      msg_error("Kafka producer is not set up properly, topic name is missing",
		evt_tag_str("driver", self->super.super.super.id),
                NULL);
      return FALSE;
    }

  msg_verbose("Initializing Kafka destination",
              evt_tag_str("driver", self->super.super.super.id),
              NULL);


  if (self->payload == NULL)
    {
      self->payload = log_template_new(cfg, "default_kafka_template");
      log_template_compile(self->payload, "$MESSAGE", NULL);
    }


  return log_threaded_dest_driver_start_workers(&self->super);
}

static void
kafka_dd_free(LogPipe *d)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  log_template_options_destroy(&self->template_options);

  log_template_unref(self->payload);
  if (self->topic)
    rd_kafka_topic_destroy(self->topic);
  if (self->kafka)
    rd_kafka_destroy(self->kafka);
  if (self->topic_name)
    g_free(self->topic_name);
  kafka_property_list_free(self->props);
  kafka_property_list_free(self->topic_props);
  log_threaded_dest_driver_free(d);
}

/*
 * Plugin glue.
 */

LogDriver *
kafka_dd_new(GlobalConfig *cfg)
{
  KafkaDestDriver *self = g_new0(KafkaDestDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);
  self->super.super.super.super.init = kafka_dd_init;
  self->super.super.super.super.free_fn = kafka_dd_free;
  self->super.super.super.super.generate_persist_name = kafka_dd_format_persist_name;

  self->super.format_stats_instance = kafka_dd_format_stats_instance;
  self->super.stats_source = SCS_KAFKA;
  self->super.worker.construct = _construct_worker;

  self->flags = KAFKA_FLAG_NONE;

  log_template_options_defaults(&self->template_options);

  return (LogDriver *)self;
}
