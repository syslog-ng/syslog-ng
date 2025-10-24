/*
 * Copyright (c) 2020 Balabit
 * Copyright (c) 2020 Balazs Scheidler
 * Copyright (c) 2020 Vivin Peris
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

#include "logthrdest/logthrdestdrv.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#include <librdkafka/rdkafka.h>
#pragma GCC diagnostic pop
#include "kafka-dest-driver.h"
#include "kafka-dest-worker.h"

#define TOPIC_NAME_ERROR topic_name_error_quark()

typedef enum _KafkaTopicError
{
  TOPIC_LENGTH_ZERO,
  TOPIC_DOT_TWO_DOTS,
  TOPIC_EXCEEDS_MAX_LENGTH,
  TOPIC_INVALID_PATTERN,
} KafkaTopicError;

GQuark topic_name_error_quark(void);

gboolean kafka_is_valid_topic_pattern(const gchar *name);
gboolean kafka_validate_topic_name(const gchar *name, GError **error);
gboolean kafka_conf_get_prop(const rd_kafka_conf_t *conf, const gchar *name, gchar *dest, size_t *dest_size);
gboolean kafka_conf_set_prop(rd_kafka_conf_t *conf, const gchar *name, const gchar *value);
gboolean kafka_apply_config_props(rd_kafka_conf_t *conf, GList *props, gchar **protected_properties,
                                  gsize protected_properties_num);
void kafka_log_partition_list(const rd_kafka_topic_partition_list_t *partitions);
void kafka_log_callback(const rd_kafka_t *rkt, int level, const char *fac, const char *msg);


/* Kafka Destination */

struct _KafkaDestWorker
{
  LogThreadedDestWorker super;
  struct iv_timer poll_timer;
  GString *key;
  GString *message;
  GString *topic_name_buffer;
};

struct _KafkaDestDriver
{
  LogThreadedDestDriver super;

  LogTemplateOptions template_options;
  LogTemplate *key;
  LogTemplate *message;
  LogTemplate *topic_name;
  GHashTable *topics;
  GMutex topics_lock;

  gboolean transaction_commit;
  GList *config;
  gchar *bootstrap_servers;
  gchar *fallback_topic_name;
  rd_kafka_topic_t *topic;
  rd_kafka_t *kafka;
  gint flush_timeout_on_shutdown;
  gint flush_timeout_on_reload;
  gint poll_timeout;
  gboolean transaction_inited;
};

const gchar *kafka_dest_worker_resolve_template_topic_name(KafkaDestWorker *self, LogMessage *msg);
rd_kafka_topic_t *kafka_dest_worker_calculate_topic_from_template(KafkaDestWorker *self, LogMessage *msg);
rd_kafka_topic_t *kafka_dest_worker_get_literal_topic(KafkaDestWorker *self);
rd_kafka_topic_t *kafka_dest_worker_calculate_topic(KafkaDestWorker *self, LogMessage *msg);
rd_kafka_topic_t *kafka_dd_query_insert_topic(KafkaDestDriver *self, const gchar *name);
gboolean kafka_dd_init(LogPipe *s);

#endif

