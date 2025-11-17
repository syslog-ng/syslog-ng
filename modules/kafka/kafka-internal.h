/*
 * Copyright (c) 2020 Balabit
 * Copyright (c) 2020 Balazs Scheidler
 * Copyright (c) 2020 Vivin Peris
 * Copyright (c) 2025 Hofi <hofione@gmail.com>
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

#ifndef KAFKA_INTERNAL_H_INCLUDED
#define KAFKA_INTERNAL_H_INCLUDED

#include "logthrsource/logthrfetcherdrv.h"
#include "logthrdest/logthrdestdrv.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#include <librdkafka/rdkafka.h>
#pragma GCC diagnostic pop
#include "kafka-source-driver.h"
#include "kafka-source-worker.h"
#include "kafka-dest-driver.h"
#include "kafka-dest-worker.h"

#if SYSLOG_NG_ENABLE_DEBUG
#define kafka_msg_debug msg_verbose
#else
#define kafka_msg_debug msg_debug
#endif

#define TOPIC_NAME_ERROR topic_name_error_quark()

typedef enum _KafkaLogging
{
  KFL_DISABLED,
  KFL_KAFKA_LEVEL,
  KFL_TRACE_LEVEL,

  KFL_UNKNOWN
} KafkaLogging;

typedef enum _KafkaConnectedState
{
  KFS_CONNECTED,
  KFS_DISCONNECTED,

  KFS_UNKNOWN
} KafkaConnectedState;

typedef enum _KafkaTopicError
{
  TOPIC_LENGTH_ZERO,
  TOPIC_DOT_TWO_DOTS,
  TOPIC_EXCEEDS_MAX_LENGTH,
  TOPIC_INVALID_PATTERN,
} KafkaTopicError;

GQuark topic_name_error_quark(void);

gboolean kafka_validate_topic_pattern(const char *topic, GError **error);
gboolean kafka_validate_topic_name(const gchar *name, GError **error);
gboolean kafka_is_valid_topic_name_pattern(const gchar *name);
gboolean kafka_conf_get_prop(const rd_kafka_conf_t *conf, const gchar *name, gchar *dest, size_t *dest_size);
gboolean kafka_conf_set_prop(rd_kafka_conf_t *conf, const gchar *name, const gchar *value);
gboolean kafka_apply_config_props(rd_kafka_conf_t *conf, GList *props, gchar **protected_properties,
                                  gsize protected_properties_num);
void kafka_log_partition_list(const rd_kafka_topic_partition_list_t *partitions);
void kafka_log_callback(const rd_kafka_t *rkt, int level, const char *fac, const char *msg);

void kafka_register_counters(KafkaSourceDriver *self, GHashTable *stats_table, const gchar *label,
                             const gchar *label_value, const gchar **counter_names, gint level);
void kafka_unregister_counters(KafkaSourceDriver *self, const gchar *label, const gchar *label_value,
                               StatsCounterItem *counter, const gchar **counter_names);

rd_kafka_resp_err_t kafka_update_state(KafkaSourceDriver *self, gboolean lock);
void kafka_final_flush(KafkaSourceDriver *self, gboolean commit);

typedef struct _KafkaOptions
{
  gchar *bootstrap_servers;
  GList *config;
  KafkaLogging kafka_logging;
  gint poll_timeout;
  gint state_update_timeout;
} KafkaOptions;

void kafka_options_defaults(KafkaOptions *self);
void kafka_options_destroy(KafkaOptions *self);
void kafka_options_merge_config(KafkaOptions *self, GList *props);
gboolean kafka_options_set_logging(KafkaOptions *self, const gchar *logging);
void kafka_options_set_bootstrap_servers(KafkaOptions *self, const gchar *bootstrap_servers);
void kafka_options_set_poll_timeout(KafkaOptions *self, gint poll_timeout);
void kafka_options_set_state_update_timeout(KafkaOptions *self, gint state_update_timeout);

typedef struct _KafkaState
{
  GMutex mutex;
  KafkaConnectedState state;
  gint last_error;
} KafkaState;

typedef struct _KafkaOpaque
{
  LogDriver *driver;
  KafkaOptions *options;
  KafkaState state;
} KafkaOpaque;

void kafka_opaque_init(KafkaOpaque *self, LogDriver *driver, KafkaOptions *options);
void kafka_opaque_deinit(KafkaOpaque *self);
LogDriver *kafka_opaque_driver(KafkaOpaque *self);
KafkaOptions *kafka_opaque_options(KafkaOpaque *self);
void kafka_opaque_state_lock(KafkaOpaque *self);
void kafka_opaque_state_unlock(KafkaOpaque *self);
KafkaConnectedState kafka_opaque_state_get(KafkaOpaque *self);
void kafka_opaque_state_set(KafkaOpaque *self, KafkaConnectedState state);
gint kafka_opaque_state_get_last_error(KafkaOpaque *self);
void kafka_opaque_state_set_last_error(KafkaOpaque *self, gint error);

/* Kafka Source */

typedef enum _KafkaSrcConsumerStrategy
{
  KSCS_ASSIGN,
  KSCS_SUBSCRIBE,

  KSCS_UNDEFINED
} KafkaSrcConsumerStrategy;

struct _KafkaSourceOptions
{
  KafkaOptions super;
  /* WARNING: multiple inheritance! */
  LogThreadedSourceWorkerOptions *worker_options;

  MsgFormatOptions *format_options;
  LogTemplateOptions template_options;

  GList *requested_topics;
  KafkaSrcConsumerStrategy strategy_hint;
  LogTemplate *message;
  gint time_reopen;

  gboolean do_not_use_bookmark;
  guint fetch_delay;
  guint fetch_retry_delay;
  guint fetch_limit; // TODO: use together with "queued.max.messages.kbytes", if 0 kafka's own setting is used automatically
  guint fetch_queue_full_delay;
  gboolean separated_worker_queues;
};

struct _KafkaSourceWorker
{
  LogThreadedSourceWorker super;
  gchar name[32]; /* see kafka_src_worker_new why a fixed size name buffer */
};

const gchar *kafka_src_worker_get_name(LogThreadedSourceWorker *worker);

struct _KafkaSourceDriver
{
  LogThreadedSourceDriver super;
  KafkaSourceOptions options;
  KafkaOpaque opaque;

  rd_kafka_t *kafka;
  rd_kafka_queue_t *consumer_kafka_queue;
  rd_kafka_queue_t *main_kafka_queue;
  rd_kafka_topic_partition_list_t *assigned_partitions;

  gchar *group_id;
  GList *requested_topics;

  KafkaSrcConsumerStrategy strategy;
  GAsyncQueue **msg_queues;
  GCond *queue_conds;
  GMutex *queue_cond_mutexes;
  guint allocated_queue_num;
  gchar single_queue_name[64];

  GAtomicCounter running_thread_num;
  GAtomicCounter sleeping_thread_num;

  const gchar *persist_name;
  const gchar *stat_persist_name;

  GHashTable *stats_topics;
  GHashTable *stats_workers;
  StatsAggregator *max_message_size;
  StatsAggregator *average_messages_size;
  StatsAggregator *CPS;

};

void kafka_sd_options_defaults(KafkaSourceOptions *self,
                               LogThreadedSourceWorkerOptions *worker_options);
void kafka_sd_options_destroy(KafkaSourceOptions *self);

gboolean kafka_sd_reopen(LogDriver *s);

guint kafka_sd_worker_queues_len(KafkaSourceDriver *self);
GAsyncQueue *kafka_sd_worker_queue(KafkaSourceDriver *self, LogThreadedSourceWorker *worker);
void kafka_sd_wait_for_queue(KafkaSourceDriver *self, LogThreadedSourceWorker *worker);
void kafka_sd_signal_queue(KafkaSourceDriver *self, LogThreadedSourceWorker *worker);
void kafka_sd_signal_queue_ndx(KafkaSourceDriver *self, guint ndx);
void kafka_sd_signal_queues(KafkaSourceDriver *self);
void kafka_sd_drop_queued_messages(KafkaSourceDriver *self);
void kafka_sd_wakeup_kafka_queues(KafkaSourceDriver *self);

void kafka_sd_update_msg_length_stats(KafkaSourceDriver *self, gsize len);
void kafka_sd_inc_msg_topic_stats(KafkaSourceDriver *self, const gchar *topic);
void kafka_sd_update_msg_worker_stats(KafkaSourceDriver *self, gint worker_ndx);

/* Kafka Destination */

struct _KafkaDestWorker
{
  LogThreadedDestWorker super;
  struct iv_timer poll_timer;
  GString *key;
  GString *message;
  GString *topic_name_buffer;
};

struct _KafkaDestinationOptions
{
  KafkaOptions super;

  LogTemplate *topic_name;
  gchar *fallback_topic_name;

  LogTemplateOptions template_options;
  LogTemplate *key;
  LogTemplate *message;

  gint flush_timeout_on_shutdown;
  gint flush_timeout_on_reload;

  gboolean transaction_commit;
};

struct _KafkaDestDriver
{
  LogThreadedDestDriver super;
  KafkaDestinationOptions options;
  KafkaOpaque opaque;

  rd_kafka_topic_t *topic;
  rd_kafka_t *kafka;

  GHashTable *topics;
  GMutex topics_lock;

  gboolean transaction_inited;
};

const gchar *kafka_dest_worker_resolve_template_topic_name(KafkaDestWorker *self, LogMessage *msg);
rd_kafka_topic_t *kafka_dest_worker_calculate_topic_from_template(KafkaDestWorker *self, LogMessage *msg);
rd_kafka_topic_t *kafka_dest_worker_get_literal_topic(KafkaDestWorker *self);
rd_kafka_topic_t *kafka_dest_worker_calculate_topic(KafkaDestWorker *self, LogMessage *msg);
rd_kafka_topic_t *kafka_dd_query_insert_topic(KafkaDestDriver *self, const gchar *name);
gboolean kafka_dd_init(LogPipe *s);

#endif

