/*
 * Copyright (c) 2013 Tihamer Petrovics <tihameri@gmail.com>
 * Copyright (c) 2014 Pierre-Yves Ritschard <pyr@spootnik.org>
 * Copyright (c) 2013-2019 Balabit
 * Copyright (c) 2019 Balazs Scheidler
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
#include "kafka-internal.h"
#include "kafka-props.h"

#include <stdlib.h>

/*
 * Configuration
 */

void
kafka_dd_set_topic(LogDriver *d, LogTemplate *topic)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  log_template_unref(self->options.topic_name);
  self->options.topic_name = topic;
}

void
kafka_dd_set_fallback_topic(LogDriver *d, const gchar *fallback_topic)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  g_free(self->options.fallback_topic_name);
  self->options.fallback_topic_name = g_strdup(fallback_topic);
}

void
kafka_dd_merge_config(LogDriver *d, GList *props)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  kafka_options_merge_config(&self->options.super, props);
}

gboolean
kafka_dd_set_logging(LogDriver *d, const gchar *logging)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  return kafka_options_set_logging(&self->options.super, logging);
}

void
kafka_dd_set_bootstrap_servers(LogDriver *d, const gchar *bootstrap_servers)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  kafka_options_set_bootstrap_servers(&self->options.super, bootstrap_servers);
}

void
kafka_dd_set_poll_timeout(LogDriver *d, gint poll_timeout)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  kafka_options_set_poll_timeout(&self->options.super, poll_timeout);
}

void
kafka_dd_set_key_ref(LogDriver *d, LogTemplate *key)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  log_template_unref(self->options.key);
  self->options.key = key;
}

void
kafka_dd_set_message_ref(LogDriver *d, LogTemplate *message)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  log_template_unref(self->options.message);
  self->options.message = message;
}

void
kafka_dd_set_flush_timeout_on_shutdown(LogDriver *d, gint flush_timeout_on_shutdown)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  self->options.flush_timeout_on_shutdown = flush_timeout_on_shutdown;
}

void
kafka_dd_set_flush_timeout_on_reload(LogDriver *d, gint flush_timeout_on_reload)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  self->options.flush_timeout_on_reload = flush_timeout_on_reload;
}

void
kafka_dd_set_transaction_commit(LogDriver *d, gboolean transaction_commit)
{
#if ! SYSLOG_NG_HAVE_RD_KAFKA_INIT_TRANSACTIONS
  msg_warning_once("syslog-ng version does not support transactional api, because the librdkafka version does not support it");
#else
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  self->options.transaction_commit = transaction_commit;
#endif
}

LogTemplateOptions *
kafka_dd_get_template_options(LogDriver *d)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  return &self->options.template_options;
}

gboolean
kafka_dd_is_topic_name_a_template(KafkaDestDriver *self)
{
  return self->topic == NULL;
}

/* methods */

static const gchar *
_format_stats_key(LogThreadedDestDriver *d, StatsClusterKeyBuilder *kb)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "kafka"));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("topic", self->options.topic_name->template_str));

  return NULL;
}

static const gchar *
_format_persist_name(const LogPipe *d)
{
  const KafkaDestDriver *self = (const KafkaDestDriver *)d;
  static gchar persist_name[1024];

  if (d->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "kafka.%s", d->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "kafka(%s)", self->options.topic_name->template_str);
  return persist_name;
}

rd_kafka_topic_t *
_construct_topic(KafkaDestDriver *self, const gchar *name)
{
  g_assert(self->kafka != NULL);

  GError *error = NULL;

  if (kafka_validate_topic_name(name, &error))
    {
      return rd_kafka_topic_new(self->kafka, name, NULL);
    }

  msg_error("Error constructing topic", evt_tag_str("topic_name", name),
            evt_tag_str("driver", self->super.super.super.id),
            log_pipe_location_tag(&self->super.super.super.super),
            evt_tag_str("error message", error->message));
  g_error_free(error);

  return NULL;
}

rd_kafka_topic_t *
kafka_dd_query_insert_topic(KafkaDestDriver *self, const gchar *name)
{
  g_mutex_lock(&self->topics_lock);
  rd_kafka_topic_t *topic = g_hash_table_lookup(self->topics, name);

  if (topic)
    {
      g_mutex_unlock(&self->topics_lock);
      return topic;
    }

  topic = _construct_topic(self, name);

  if (topic)
    {
      g_hash_table_insert(self->topics, g_strdup(name), topic);
    }

  g_mutex_unlock(&self->topics_lock);
  return topic;
}

static void
_kafka_delivery_report_cb(rd_kafka_t *rk,
                          void *payload, size_t len,
                          rd_kafka_resp_err_t err,
                          void *opaque, void *msg_opaque)
{
  KafkaDestDriver *self = (KafkaDestDriver *) ((KafkaOpaque *)opaque)->driver;;

  /* delivery callback will be called from the the thread where rd_kafka_poll is called,
   * which could be any worker and not just worker#0 due to the kafka_dd_shutdown in thread_init
   * and the main thread too. Driver/worker state modification should be done carefully.
   */
  if (err != RD_KAFKA_RESP_ERR_NO_ERROR)
    {
      msg_debug("kafka: delivery report for message came back with an error, message is lost",
                evt_tag_str("topic", self->options.topic_name->template_str),
                evt_tag_str("fallback_topic", self->options.fallback_topic_name),
                evt_tag_mem("message", (char *) payload, MIN(len, 128)),
                evt_tag_str("error", rd_kafka_err2str(err)),
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
    }
  else
    {
      msg_debug("kafka: delivery report successful",
                evt_tag_str("topic", self->options.topic_name->template_str),
                evt_tag_str("fallback_topic", self->options.fallback_topic_name),
                evt_tag_mem("message", (char *) payload, MIN(len, 128)),
                evt_tag_str("error", rd_kafka_err2str(err)),
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
    }
}

/*
 * Main thread
 */

static rd_kafka_t *
_construct_client(KafkaDestDriver *self)
{
  rd_kafka_t *client;
  rd_kafka_conf_t *conf;
  gchar errbuf[1024];

  conf = rd_kafka_conf_new();
  if (!kafka_conf_set_prop(conf, "metadata.broker.list", self->options.super.bootstrap_servers))
    return NULL;
  if (!kafka_conf_set_prop(conf, "topic.partitioner", "murmur2_random"))
    return NULL;

  if (self->options.transaction_commit)
    kafka_conf_set_prop(conf, "transactional.id",
                        log_pipe_get_persist_name(&self->super.super.super.super));

  static gchar *protected_properties[] =
  {
    "bootstrap.servers",
    "metadata.broker.list",
  };
  if (!kafka_apply_config_props(conf, self->options.super.config, protected_properties,
                                G_N_ELEMENTS(protected_properties)))
    return NULL;

  if (self->options.super.kafka_logging != KFL_DISABLED)
    rd_kafka_conf_set_log_cb(conf, kafka_log_callback);
  rd_kafka_conf_set_dr_cb(conf, _kafka_delivery_report_cb);
  rd_kafka_conf_set_opaque(conf, &self->opaque);

  client = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errbuf, sizeof(errbuf));
  if (!client)
    {
      msg_error("kafka: error constructing the kafka connection object",
                evt_tag_str("topic", self->options.topic_name->template_str),
                evt_tag_str("error", errbuf),
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
    }
  return client;
}

static LogThreadedDestWorker *
_construct_worker(LogThreadedDestDriver *s, gint worker_index)
{
  return kafka_dest_worker_new(s, worker_index);
}

static gint
_get_flush_timeout(KafkaDestDriver *self)
{
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super.super);
  if (cfg_is_shutting_down(cfg))
    return self->options.flush_timeout_on_shutdown;
  return self->options.flush_timeout_on_reload;
}

static void
_flush_inflight_messages(KafkaDestDriver *self)
{
  rd_kafka_resp_err_t err;
  gint outq_len = rd_kafka_outq_len(self->kafka);
  gint timeout_ms = _get_flush_timeout(self);

  if (outq_len > 0)
    {
      msg_notice("kafka: shutting down kafka producer, while messages are still in-flight, waiting for messages to flush",
                 evt_tag_str("topic", self->options.topic_name->template_str),
                 evt_tag_str("fallback_topic", self->options.fallback_topic_name),
                 evt_tag_int("outq_len", outq_len),
                 evt_tag_int("timeout_ms", timeout_ms),
                 evt_tag_str("driver", self->super.super.super.id),
                 log_pipe_location_tag(&self->super.super.super.super));
    }
  err = rd_kafka_flush(self->kafka, timeout_ms);
  if (err != RD_KAFKA_RESP_ERR_NO_ERROR)
    {
      msg_error("kafka: error flushing accumulated messages during shutdown, rd_kafka_flush() returned failure, this might indicate that some in-flight messages are lost",
                evt_tag_str("topic", self->options.topic_name->template_str),
                evt_tag_str("fallback_topic", self->options.fallback_topic_name),
                evt_tag_int("outq_len", rd_kafka_outq_len(self->kafka)),
                evt_tag_str("error", rd_kafka_err2str(err)),
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
    }
  outq_len = rd_kafka_outq_len(self->kafka);

  if (outq_len != 0)
    msg_notice("kafka: timeout while waiting for the librdkafka queue to empty, the "
               "remaining entries will be purged and lost",
               evt_tag_int("timeout_ms", timeout_ms),
               evt_tag_int("outq_len", outq_len));
}

static void
_purge_remaining_messages(KafkaDestDriver *self)
{
  /* we are purging all messages, those ones that are sitting in the queue
   * and also those that were sent and not yet acknowledged.  The purged
   * messages will generate failed delivery reports. */

  rd_kafka_purge(self->kafka, RD_KAFKA_PURGE_F_QUEUE | RD_KAFKA_PURGE_F_INFLIGHT);
  rd_kafka_poll(self->kafka, 0);
}

static gboolean
_init_template_topic_name(KafkaDestDriver *self)
{
  msg_debug("kafka: The topic name is a template",
            evt_tag_str("topic", self->options.topic_name->template_str),
            evt_tag_str("driver", self->super.super.super.id),
            log_pipe_location_tag(&self->super.super.super.super));

  if (!self->options.fallback_topic_name)
    {
      msg_error("kafka: fallback_topic() required when the topic name is a template",
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
      return FALSE;
    }

  rd_kafka_topic_t *fallback_topic = _construct_topic(self, self->options.fallback_topic_name);

  if (fallback_topic == NULL)
    {
      msg_error("kafka: error constructing the fallback topic object",
                evt_tag_str("fallback_topic", self->options.fallback_topic_name),
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
      return FALSE;
    }

  self->topics = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) rd_kafka_topic_destroy);
  g_hash_table_insert(self->topics, g_strdup(self->options.fallback_topic_name), fallback_topic);

  return TRUE;
}

static gboolean
_topic_name_is_a_template(KafkaDestDriver *self)
{
  return !log_template_is_literal_string(self->options.topic_name);
}

static gboolean
_init_literal_topic_name(KafkaDestDriver *self)
{
  self->topic = _construct_topic(self, self->options.topic_name->template_str);

  if (self->topic == NULL)
    {
      msg_error("kafka: error constructing the kafka topic object",
                evt_tag_str("topic", self->options.topic_name->template_str),
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_init_topic_name(KafkaDestDriver *self)
{
  if (_topic_name_is_a_template(self))
    return _init_template_topic_name(self);
  else
    return _init_literal_topic_name(self);
}

static void
_destroy_kafka(LogDriver *s)
{
  KafkaDestDriver *self = (KafkaDestDriver *)s;

  if (self->topics)
    g_hash_table_unref(self->topics);
  if (self->topic)
    rd_kafka_topic_destroy(self->topic);

  if (self->kafka)
    rd_kafka_destroy(self->kafka);
}

gboolean
kafka_dd_reopen(LogDriver *s)
{
  KafkaDestDriver *self = (KafkaDestDriver *)s;
  _destroy_kafka(s);

  self->kafka = _construct_client(self);
  if (self->kafka == NULL)
    {
      msg_error("kafka: error constructing kafka connection object",
                evt_tag_str("topic", self->options.topic_name->template_str),
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
      return FALSE;
    }
  if (!_init_topic_name(self))
    return FALSE;

  self->transaction_inited = FALSE;

  return TRUE;
}

gboolean
kafka_dd_init(LogPipe *s)
{
  KafkaDestDriver *self = (KafkaDestDriver *)s;

  if (!self->options.topic_name)
    {
      msg_error("kafka: the topic() argument is required for kafka destinations",
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
      return FALSE;
    }
  if (!self->options.super.bootstrap_servers)
    {
      msg_error("kafka: the bootstrap-servers() option is required for kafka destinations",
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
      return FALSE;
    }

  if (!kafka_dd_reopen(&self->super.super.super))
    {
      return FALSE;
    }


  if (self->options.transaction_commit)
    {
      /*
       * The transaction api works on the rd_kafka client level,
       * and packing multiple kafka operation into an atomic one.
       * But it bundles them by calling the same operations,
       * there is no way to selectivly bundle calls.
       * Thus a worker cannot have its own dedicated transaction.
       *
       */
      if (self->super.num_workers > 1)
        {
          msg_info("kafka: in case of sync_send(yes) option the number of workers limited to 1", evt_tag_int("configured_workers",
                   self->super.num_workers), evt_tag_int("workers", 1));
          log_threaded_dest_driver_set_num_workers(&self->super.super.super, 1);
        }

    }

  if (!log_threaded_dest_driver_init_method(s))
    return FALSE;

  GlobalConfig *cfg = log_pipe_get_config(s);
  if (self->options.message == NULL)
    {
      self->options.message = log_template_new(cfg, NULL);
      log_template_compile(self->options.message, "$ISODATE $HOST $MSGHDR$MSG", NULL);
    }

  log_template_options_init(&self->options.template_options, cfg);
  msg_verbose("kafka: Kafka destination initialized",
              evt_tag_str("topic", self->options.topic_name->template_str),
              evt_tag_str("fallback_topic", self->options.fallback_topic_name),
              evt_tag_str("key", self->options.key ? self->options.key->template_str : "NULL"),
              evt_tag_str("message", self->options.message->template_str),
              evt_tag_str("driver", self->super.super.super.id),
              log_pipe_location_tag(&self->super.super.super.super));

  return TRUE;
}

void
kafka_dd_shutdown(LogThreadedDestDriver *s)
{
  KafkaDestDriver *self = (KafkaDestDriver *)s;

  /* this can be called from the worker threads and
   * during reloda/shutdown from the main thread (deinit)
   * No need to lock here, as librdkafka API is thread safe, it does the locking for us.
   */

  _flush_inflight_messages(self);
  _purge_remaining_messages(self);
}

static void
_check_for_remaining_messages(KafkaDestDriver *self)
{
  gint outq_len = rd_kafka_outq_len(self->kafka);
  if (outq_len != 0)
    msg_notice("kafka: failed to completely empty rdkafka queues, as we still have entries in "
               "the queue after flush() and purge(), this is probably causing a memory leak, "
               "please contact syslog-ng authors for support",
               evt_tag_int("outq_len", outq_len));
}

static gboolean
kafka_dd_deinit(LogPipe *s)
{
  KafkaDestDriver *self = (KafkaDestDriver *)s;

  kafka_dd_shutdown(&self->super);
  _check_for_remaining_messages(self);

  return log_threaded_dest_driver_deinit_method(s);
}

static void
kafka_dd_free(LogPipe *d)
{
  KafkaDestDriver *self = (KafkaDestDriver *)d;

  _destroy_kafka(&self->super.super.super);
  if (self->options.fallback_topic_name)
    g_free(self->options.fallback_topic_name);
  log_template_options_destroy(&self->options.template_options);
  log_template_unref(self->options.key);
  log_template_unref(self->options.message);
  log_template_unref(self->options.topic_name);
  g_mutex_clear(&self->topics_lock);
  kafka_options_destroy(&self->options.super);
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
  self->super.super.super.super.generate_persist_name = _format_persist_name;

  self->super.format_stats_key = _format_stats_key;
  self->super.stats_source = stats_register_type("kafka");
  self->super.worker.construct = _construct_worker;

  self->opaque.driver = &self->super.super.super;
  self->opaque.options = &self->options.super;
  g_mutex_init(&self->topics_lock);

  /* one minute */
  self->options.flush_timeout_on_shutdown = 60000;
  self->options.flush_timeout_on_reload = 1000;

  kafka_options_defaults(&self->options.super);
  log_template_options_defaults(&self->options.template_options);

  return (LogDriver *)self;
}
