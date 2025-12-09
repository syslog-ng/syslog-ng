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

#include "kafka-internal.h"
#include "kafka-props.h"

GQuark
topic_name_error_quark(void)
{
  return g_quark_from_static_string("invalid-topic-name-error-quark");
}

gboolean
kafka_is_valid_topic_name_pattern(const gchar *name)
{
  const gchar *p;
  for (p = name; *p; p++)
    {
      if (!((*p >= 'a' && *p <= 'z') ||
            (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') ||
            (*p == '_') || (*p == '-') || (*p == '.')))
        {
          return FALSE;
        }
    }
  return TRUE;
}

gboolean
kafka_validate_topic_pattern(const char *topic, GError **error)
{
  if (topic == NULL || *topic == 0)
    return FALSE;

  regex_t re;
  int ret = regcomp(&re, topic, REG_EXTENDED | REG_NOSUB);
  if (ret == 0)
    {
      regfree(&re);
      return TRUE;
    }
  if (error)
    g_set_error(error, TOPIC_NAME_ERROR, TOPIC_INVALID_PATTERN,
                "kafka: topic name %s is illegal as it contains a badly formatted regex pattern", topic);
  return FALSE;
}

gboolean
kafka_validate_topic_name(const gchar *name, GError **error)
{
  gint len = strlen(name);

  if (len == 0)
    {
      if (error)
        g_set_error(error, TOPIC_NAME_ERROR, TOPIC_LENGTH_ZERO,
                    "kafka: topic name is illegal, it can't be empty");
      return FALSE;
    }

  if ((!g_strcmp0(name, ".")) || !g_strcmp0(name, ".."))
    {
      if (error)
        g_set_error(error, TOPIC_NAME_ERROR, TOPIC_DOT_TWO_DOTS,
                    "kafka: topic name cannot be . or ..");
      return FALSE;
    }

  if (len > 249)
    {
      if (error)
        g_set_error(error, TOPIC_NAME_ERROR, TOPIC_EXCEEDS_MAX_LENGTH,
                    "kafka: topic name cannot be longer than 249 characters");
      return FALSE;
    }

  if (FALSE == kafka_is_valid_topic_name_pattern(name))
    {
      if (error)
        g_set_error(error, TOPIC_NAME_ERROR, TOPIC_INVALID_PATTERN,
                    "kafka: topic name %s is illegal as it contains characters other than pattern [-._a-zA-Z0-9]+", name);
      return FALSE;
    }

  return TRUE;
}

void
kafka_log_callback(const rd_kafka_t *rkt, int level, const char *fac, const char *msg)
{
  /* NOTE: you cannot use KafkaOpaque driver here, as you do not know the type of the driver */
  KafkaOptions *options = (KafkaOptions *) ((KafkaOpaque *)rd_kafka_opaque(rkt))->options;

  gchar *buf = g_strdup_printf("librdkafka: %s(%d): %s", fac, level, msg);

  if (options->kafka_logging == KFL_KAFKA_LEVEL)
    msg_event_send(msg_event_create(level, buf, NULL));
  else
    msg_trace("kafka: log callback message", evt_tag_str("msg", buf));
  g_free(buf);
}

gboolean
kafka_conf_get_prop(const rd_kafka_conf_t *conf, const gchar *name, gchar *dest, size_t *dest_size)
{
  rd_kafka_conf_res_t res;

  if ((res = rd_kafka_conf_get(conf, name, dest, dest_size)) != RD_KAFKA_CONF_OK)
    {
      msg_error("kafka: error getting librdkafka config property",
                evt_tag_str("name", name),
                evt_tag_int("error", res));
      return FALSE;
    }
  return TRUE;
}

gboolean
kafka_conf_set_prop(rd_kafka_conf_t *conf, const gchar *name, const gchar *value)
{
  gchar errbuf[1024];

  kafka_msg_debug("kafka: setting librdkafka config property",
                  evt_tag_str("name", name),
                  evt_tag_str("value", value));
  if (rd_kafka_conf_set(conf, name, value, errbuf, sizeof(errbuf)) < 0)
    {
      msg_error("kafka: error setting librdkafka config property",
                evt_tag_str("name", name),
                evt_tag_str("value", value),
                evt_tag_str("error", errbuf));
      return FALSE;
    }
  return TRUE;
}

static gboolean
_is_property_protected(const gchar *property_name, gchar **protected_properties, gsize protected_properties_num)
{
  for (gint i = 0; i < protected_properties_num; i++)
    {
      if (strcmp(property_name, protected_properties[i]) == 0)
        {
          msg_warning("kafka: protected config properties cannot be overridden",
                      evt_tag_str("name", property_name));
          return TRUE;
        }
    }
  return FALSE;
}

gboolean
kafka_apply_config_props(rd_kafka_conf_t *conf, GList *props, gchar **protected_properties,
                         gsize protected_properties_num)
{
  GList *ll;

  for (ll = props; ll != NULL; ll = g_list_next(ll))
    {
      KafkaProperty *kp = ll->data;
      if (!_is_property_protected(kp->name, protected_properties, protected_properties_num))
        if (!kafka_conf_set_prop(conf, kp->name, kp->value))
          return FALSE;
    }
  return TRUE;
}

inline gchar *
kafka_format_partition_key(const gchar *topic, int32_t partition, gchar *key, gsize key_size)
{
  g_snprintf(key, key_size, "%s#%d", topic, partition);
  return key;
}

gboolean
kafka_seek_partition(KafkaSourceDriver *self,
                     rd_kafka_topic_partition_t *partition,
                     int64_t offset,
                     int timeout_ms)
{
  gboolean success = TRUE;
  /* rd_kafka_seek() needs a rd_kafka_topic_t*, so create a temporary handle */
  rd_kafka_topic_t *rkt = rd_kafka_topic_new(self->kafka, partition->topic, NULL);
  if (!rkt)
    {
      msg_error("kafka: rd_kafka_topic_new() failed in fallback seek",
                evt_tag_str("topic", partition->topic),
                evt_tag_str("error", rd_kafka_err2str(rd_kafka_last_error())));
      return FALSE;
    }

  rd_kafka_resp_err_t err = rd_kafka_seek(rkt, partition->partition, offset, timeout_ms);
  if (err != RD_KAFKA_RESP_ERR_NO_ERROR)
    {
      msg_error("kafka: failed to seek to the restored offset (legacy rd_kafka_seek)",
                evt_tag_str("topic", partition->topic),
                evt_tag_int("partition", (int)partition->partition),
                evt_tag_long("offset", offset),
                evt_tag_str("error", rd_kafka_err2str(err)));
      success = FALSE;
    }
  rd_kafka_topic_destroy(rkt);
  return success;
}

gboolean
kafka_seek_partitions(KafkaSourceDriver *self,
                      rd_kafka_topic_partition_list_t *partitions,
                      int timeout_ms)
{
  gboolean success = TRUE;

#if SYSLOG_NG_HAVE_RD_KAFKA_SEEK_PARTITIONS
  rd_kafka_error_t *seek_err = rd_kafka_seek_partitions(self->kafka, partitions, timeout_ms);
  if (seek_err)
    {
      msg_error("kafka: failed to seek to the restored offset for partitions (seek_partitions)",
                evt_tag_str("error", rd_kafka_error_string(seek_err)));
      rd_kafka_error_destroy(seek_err);
      success = FALSE;
    }
#else
  /* Fallback: call rd_kafka_seek() for every entry in the topic-partition list */
  for (int pi = 0; pi < partitions->cnt; ++pi)
    {
      rd_kafka_topic_partition_t *p = &partitions->elems[pi];
      if (FALSE == kafka_seek_partition(self, p, p->offset, timeout_ms))
        success = FALSE;
    }
#endif
  return success;
}

void
kafka_log_partition_list(KafkaSourceDriver *self, const rd_kafka_topic_partition_list_t *partitions)
{
  for (int i = 0 ; i < partitions->cnt ; i++)
    msg_verbose("kafka: partition",
                evt_tag_str("group_id", self->group_id),
                evt_tag_str("member_id", rd_kafka_memberid(self->kafka)),
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("topic", partitions->elems[i].topic),
                evt_tag_int("partition", (int) partitions->elems[i].partition),
                evt_tag_long("offset", (long) partitions->elems[i].offset));
}

void
kafka_register_counters(KafkaSourceDriver *self,
                        GHashTable *stats_table,
                        const gchar *label,
                        const gchar *label_value,
                        const gchar **counter_names,
                        gint level)
{
  LogThreadedSourceWorker *worker = self->super.workers[0];
  StatsClusterKeyBuilder *kb = worker->super.metrics.stats_kb;
  gchar *stats_id = worker->super.stats_id;
  LogSourceOptions *super_options = &self->options.worker_options->super;

  stats_lock();
  {
    stats_cluster_key_builder_push(kb);

    gchar stats_instance[1024];
    stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label(label, label_value));
    stats_cluster_key_builder_format_legacy_stats_instance(kb, stats_instance,
                                                           sizeof(stats_instance));
    StatsClusterKey sc_key;
    for (const gchar **name_ptr = counter_names; *name_ptr != NULL; name_ptr++)
      {
        stats_cluster_single_key_legacy_set_with_name(&sc_key, super_options->stats_source | SCS_SOURCE,
                                                      stats_id, stats_instance, *name_ptr);
        StatsCounterItem *counter = NULL;
        stats_register_counter(level, &sc_key, SC_TYPE_SINGLE_VALUE, &counter);
        g_hash_table_insert(stats_table, (gpointer) g_strdup(label_value), (gpointer) counter);
        kafka_msg_debug("kafka: added stats counter",
                        evt_tag_str(label, label_value),
                        evt_tag_str("counter", *name_ptr));
      }
    stats_cluster_key_builder_pop(kb);
  }
  stats_unlock();
}

void
kafka_unregister_counters(KafkaSourceDriver *self,
                          const gchar *label,
                          const gchar *label_value,
                          StatsCounterItem *counter,
                          const gchar **counter_names)
{
  LogThreadedSourceWorker *worker = self->super.workers[0];
  gchar *stats_id = worker->super.stats_id;
  StatsClusterKeyBuilder *kb = worker->super.metrics.stats_kb;
  LogSourceOptions *super_options = &self->options.worker_options->super;

  stats_lock();
  {
    stats_cluster_key_builder_push(kb);

    stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label(label, label_value));

    gchar stats_instance[1024];
    stats_cluster_key_builder_format_legacy_stats_instance(kb, stats_instance,
                                                           sizeof(stats_instance));
    for (const gchar **name_ptr = counter_names; *name_ptr != NULL; name_ptr++)
      {
        StatsClusterKey sc_key;
        stats_cluster_single_key_legacy_set_with_name(&sc_key, super_options->stats_source | SCS_SOURCE,
                                                      stats_id, stats_instance, *name_ptr);
        stats_unregister_counter(&sc_key, SC_TYPE_SINGLE_VALUE, &counter);
      }

    stats_cluster_key_builder_pop(kb);
  }
  stats_unlock();
}

void
kafka_options_defaults(KafkaOptions *self)
{
  self->poll_timeout = 10000; /* poll_timeout milliseconds - 10 seconds */
  self->state_update_timeout = 1000; /* state_update_timeout milliseconds - 1 second */
  self->kafka_logging = KFL_DISABLED;
}

void
kafka_options_destroy(KafkaOptions *self)
{
  if (self->bootstrap_servers)
    g_free(self->bootstrap_servers);
  self->bootstrap_servers = NULL;
  if (self->config)
    kafka_property_list_free(self->config);
  self->config = NULL;
}

inline void
kafka_options_merge_config(KafkaOptions *self, GList *props)
{
  self->config = g_list_concat(self->config, props);
}

KafkaLogging
kafka_string_to_logging(const gchar *logging)
{
  KafkaLogging kafka_logging = KFL_UNKNOWN;
  if (g_ascii_strcasecmp(logging, "disabled"))
    kafka_logging = KFL_DISABLED;
  else if (g_ascii_strcasecmp(logging, "kafka"))
    kafka_logging = KFL_KAFKA_LEVEL;
  else if (g_ascii_strcasecmp(logging, "trace"))
    kafka_logging = KFL_TRACE_LEVEL;
  return kafka_logging;
}

gboolean
kafka_options_set_logging(KafkaOptions *self, const gchar *logging)
{
  KafkaLogging kafka_logging = kafka_string_to_logging(logging);
  if (kafka_logging == KFL_UNKNOWN)
    return FALSE;
  self->kafka_logging = kafka_logging;
  return TRUE;
}

void
kafka_options_set_bootstrap_servers(KafkaOptions *self, const gchar *bootstrap_servers)
{
  g_free(self->bootstrap_servers);
  self->bootstrap_servers = g_strdup(bootstrap_servers);
}

inline void
kafka_options_set_poll_timeout(KafkaOptions *self, gint poll_timeout)
{
  self->poll_timeout = poll_timeout;
}

inline void
kafka_options_set_state_update_timeout(KafkaOptions *self, gint state_update_timeout)
{
  self->state_update_timeout = state_update_timeout;
}

void
kafka_opaque_init(KafkaOpaque *self, LogDriver *driver, KafkaOptions *options)
{
  self->driver = driver;
  self->options = options;
  self->state.state = KFS_UNKNOWN;
  self->state.last_error = -1;

  g_mutex_init(&self->state.mutex);
}

void
kafka_opaque_deinit(KafkaOpaque *self)
{
  self->driver = NULL;
  self->options = NULL;
  self->state.state = KFS_UNKNOWN;

  g_mutex_clear(&self->state.mutex);
}

inline LogDriver *
kafka_opaque_driver(KafkaOpaque *self)
{
  return self->driver;
}

inline KafkaOptions *
kafka_opaque_options(KafkaOpaque *self)
{
  return self->options;
}

inline void
kafka_opaque_state_lock(KafkaOpaque *self)
{
  g_mutex_lock(&self->state.mutex);
}

inline void
kafka_opaque_state_unlock(KafkaOpaque *self)
{
  g_mutex_unlock(&self->state.mutex);
}

/* NOTE: lock must be held when calling these */

inline KafkaConnectedState
kafka_opaque_state_get(KafkaOpaque *self)
{
  return self->state.state;
}

inline void
kafka_opaque_state_set(KafkaOpaque *self, KafkaConnectedState state)
{
  self->state.state = state;
}

inline gint
kafka_opaque_state_get_last_error(KafkaOpaque *self)
{
  return self->state.last_error;
}

inline void
kafka_opaque_state_set_last_error(KafkaOpaque *self, gint error)
{
  self->state.last_error = error;
}
