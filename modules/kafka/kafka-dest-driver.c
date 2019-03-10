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

/*
 * Configuration
 */

void
kafka_dd_set_topic(LogDriver *d, const gchar *topic)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  g_free(self->topic_name);
  self->topic_name = g_strdup(topic);
}

void
kafka_dd_set_global_config(LogDriver *d, GList *props)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  kafka_property_list_free(self->global_config);
  self->global_config = props;
}

void
kafka_dd_set_topic_config(LogDriver *d, GList *props)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  kafka_property_list_free(self->topic_config);
  self->topic_config = props;
}

void
kafka_dd_set_key_ref(LogDriver *d, LogTemplate *key)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  log_template_unref(self->key);
  self->key = key;
}

void
kafka_dd_set_message_ref(LogDriver *d, LogTemplate *message)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  log_template_unref(self->message);
  self->message = message;
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

static void
_conf_set_prop(rd_kafka_conf_t *conf, const gchar *name, const gchar *value)
{
  gchar errbuf[1024];

  msg_debug("Setting librdkafka Global Config property",
            evt_tag_str("name", name),
            evt_tag_str("value", value));
  if (rd_kafka_conf_set(conf, name, value, errbuf, sizeof(errbuf)) < 0)
    {
      msg_error("Error setting librdkafka Global Config property",
                evt_tag_str("name", name),
                evt_tag_str("value", value),
                evt_tag_str("error", errbuf));
    }
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
  gchar errbuf[1024];

  conf = rd_kafka_conf_new();
  for (list = g_list_first(self->global_config); list != NULL; list = g_list_next(list))
    {
      kp = list->data;
      _conf_set_prop(conf, kp->name, kp->value);
    }
  rd_kafka_conf_set_log_cb(conf, kafka_log);
  client = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errbuf, sizeof(errbuf));
  return client;
}

static void
_topic_conf_set_prop(rd_kafka_topic_conf_t *conf, const gchar *name, const gchar *value)
{
  gchar errbuf[1024];

  msg_debug("Setting librdkafka Topic Config property",
            evt_tag_str("name", name),
            evt_tag_str("value", value));
  if (rd_kafka_topic_conf_set(conf, name, value, errbuf, sizeof(errbuf)) < 0)
    {
      msg_error("Error setting librdkafka Topic Config property",
                evt_tag_str("name", name),
                evt_tag_str("value", value),
                evt_tag_str("error", errbuf));
    }
}


rd_kafka_topic_t *
_construct_topic(KafkaDestDriver *self)
{
  GList *list;
  KafkaProperty *kp;
  rd_kafka_topic_conf_t *topic_conf;

  g_assert(self->kafka != NULL);

  topic_conf = rd_kafka_topic_conf_new();
  _topic_conf_set_prop(topic_conf, "partitioner", "murmur2_random");

  for (list = g_list_first(self->topic_config); list != NULL; list = g_list_next(list))
    {
      kp = list->data;
      _topic_conf_set_prop(topic_conf, kp->name, kp->value);
    }

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


  if (self->message == NULL)
    {
      self->message = log_template_new(cfg, NULL);
      log_template_compile(self->message, "$ISODATE $HOST $MSGHDR$MSG", NULL);
    }


  return log_threaded_dest_driver_start_workers(&self->super);
}

static gboolean
kafka_dd_deinit(LogPipe *s)
{
  KafkaDestDriver *self = (KafkaDestDriver *)s;

  rd_kafka_flush(self->kafka, 60000);
  return log_threaded_dest_driver_deinit_method(s);
}

static void
kafka_dd_free(LogPipe *d)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  log_template_options_destroy(&self->template_options);

  log_template_unref(self->key);
  log_template_unref(self->message);
  if (self->topic)
    rd_kafka_topic_destroy(self->topic);
  if (self->kafka)
    rd_kafka_destroy(self->kafka);
  if (self->topic_name)
    g_free(self->topic_name);
  kafka_property_list_free(self->global_config);
  kafka_property_list_free(self->topic_config);
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
  self->super.super.super.super.deinit = kafka_dd_deinit;
  self->super.super.super.super.free_fn = kafka_dd_free;
  self->super.super.super.super.generate_persist_name = kafka_dd_format_persist_name;

  self->super.format_stats_instance = kafka_dd_format_stats_instance;
  self->super.stats_source = SCS_KAFKA;
  self->super.worker.construct = _construct_worker;

  log_template_options_defaults(&self->template_options);

  return (LogDriver *)self;
}
