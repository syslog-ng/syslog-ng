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

#include "kafka-internal.h"
#include "kafka-props.h"

GQuark
topic_name_error_quark(void)
{
  return g_quark_from_static_string("invalid-topic-name-error-quark");
}

gboolean
kafka_is_valid_topic_pattern(const gchar *name)
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
kafka_validate_topic_name(const gchar *name, GError **error)
{
  gint len = strlen(name);

  if (len == 0)
    {
      g_set_error(error, TOPIC_NAME_ERROR, TOPIC_LENGTH_ZERO,
                  "kafka: topic name is illegal, it can't be empty");
      return FALSE;
    }

  if ((!g_strcmp0(name, ".")) || !g_strcmp0(name, ".."))
    {
      g_set_error(error, TOPIC_NAME_ERROR, TOPIC_DOT_TWO_DOTS,
                  "kafka: topic name cannot be . or ..");
      return FALSE;
    }

  if (len > 249)
    {
      g_set_error(error, TOPIC_NAME_ERROR, TOPIC_EXCEEDS_MAX_LENGTH,
                  "kafka: topic name cannot be longer than 249 characters");
      return FALSE;
    }

  if (!kafka_is_valid_topic_pattern(name))
    {
      g_set_error(error, TOPIC_NAME_ERROR, TOPIC_INVALID_PATTERN,
                  "kafka: topic name %s is illegal as it contains characters other than pattern [-._a-zA-Z0-9]+", name);
      return FALSE;
    }

  return TRUE;
}

void
kafka_log_callback(const rd_kafka_t *rkt, int level, const char *fac, const char *msg)
{
  gchar *buf = g_strdup_printf("librdkafka: %s(%d): %s", fac, level, msg);
  msg_event_send(msg_event_create(level, buf, NULL));
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

  msg_debug("kafka: setting librdkafka config property",
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

void
kafka_log_partition_list(const rd_kafka_topic_partition_list_t *partitions)
{
  for (int i = 0 ; i < partitions->cnt ; i++)
    msg_verbose("kafka: partition",
                evt_tag_str("topic", partitions->elems[i].topic),
                evt_tag_int("partition", (int) partitions->elems[i].partition));
}
