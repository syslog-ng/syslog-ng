/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2023-2024 László Várady
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "stats/stats-prometheus.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster.h"
#include "stats/stats-counter.h"
#include "timeutils/unixtime.h"
#include "str-utils.h"
#include "scratch-buffers.h"

#include <string.h>


/* Exposition format:
 *
 * A label value can be any sequence of UTF-8 characters, but the
 * backslash (\), double-quote ("), and line feed (\n) characters have to be
 * escaped as \\, \", and \n, respectively.
 */
static gchar *
stats_format_prometheus_sanitize_label_value(const gchar *value)
{
  GString *sanitized_value = scratch_buffers_alloc();

  const gchar *value_end = value + strlen(value);

  while (value < value_end)
    {
      gunichar uchar = g_utf8_get_char_validated(value, value_end - value);

      if (G_UNLIKELY(uchar == (gunichar) -1 || uchar == (gunichar) -2))
        {
          /* double backslash to conform to the format */
          g_string_append_printf(sanitized_value, "\\\\x%02x", *(guint8 *) value);
          value++;
          continue;
        }

      switch (uchar)
        {
        case '\\':
          g_string_append(sanitized_value, "\\\\");
          break;
        case '"':
          g_string_append(sanitized_value, "\\\"");
          break;
        case '\n':
          g_string_append(sanitized_value, "\\n");
          break;
        default:
          g_string_append_unichar_optimized(sanitized_value, uchar);
          break;
        }

      value = g_utf8_next_char(value);
    }

  return sanitized_value->str;
}

static inline gboolean
_is_valid_name_char(gchar c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')  || (c == '_') || (c >= '0' && c <= '9');
}

static inline gboolean
_is_str_empty(const gchar *str)
{
  return !str || strcmp(str, "") == 0;
}

static gchar *
stats_format_prometheus_sanitize_name(const gchar *name)
{
  GString *sanitized_name = scratch_buffers_alloc();

  for (const gchar *c = name; *c; ++c)
    {
      if (_is_valid_name_char(*c))
        g_string_append_c(sanitized_name, *c);
      else if (*c == '.' || *c == '-')
        g_string_append_c(sanitized_name, '_');

      /* remove otherwise */
    }

  return sanitized_name->str;
}

gchar *
stats_format_prometheus_format_value(const StatsClusterKey *key, StatsCounterItem *counter)
{
  GString *value = scratch_buffers_alloc();

  gsize stored_value = stats_counter_get(counter);

  guint64 converted_int = stored_value;
  gdouble converted_double = stored_value;
  gchar double_buf[G_ASCII_DTOSTR_BUF_SIZE];

  switch (key->formatting.stored_unit)
    {
    case SCU_GIB:
      converted_int *= 1024;
    case SCU_MIB:
      converted_int *= 1024;
    case SCU_KIB:
      converted_int *= 1024;
      g_string_printf(value, "%"G_GUINT64_FORMAT, converted_int);
      break;

    case SCU_HOURS:
      converted_int *= 60;
    case SCU_MINUTES:
      converted_int *= 60;
    case SCU_SECONDS:
      if (key->formatting.frame_of_reference == SCFOR_RELATIVE_TO_TIME_OF_QUERY)
        {
          UnixTime now;
          unix_time_set_now(&now);
          converted_int = now.ut_sec - converted_int;
        }
      g_string_printf(value, "%"G_GUINT64_FORMAT, converted_int);
      break;

    case SCU_NANOSECONDS:
      converted_double /= 1e9;
      g_string_assign(value, g_ascii_dtostr(double_buf, G_N_ELEMENTS(double_buf), converted_double));
      break;

    case SCU_MILLISECONDS:
      converted_double /= 1e3;
      g_string_assign(value, g_ascii_dtostr(double_buf, G_N_ELEMENTS(double_buf), converted_double));
      break;

    default:
      /* no conversion */
      g_string_printf(value, "%"G_GSIZE_FORMAT, stored_value);
      break;
    }

  return value->str;
}

static inline void
_append_formatted_label(GString *serialized_labels, const StatsClusterLabel *label)
{
  if (!label->value)
    return;

  g_string_append_printf(serialized_labels, "%s=\"%s\"",
                         stats_format_prometheus_sanitize_name(label->name),
                         stats_format_prometheus_sanitize_label_value(label->value));
}

static inline gboolean
_is_timestamp(StatsCluster *sc, gint type)
{
  return strcmp(stats_cluster_get_type_name(sc, type), "stamp") == 0;
}

static const gchar *
_format_labels(StatsCluster *sc, gint type)
{
  StatsClusterLabel type_label;
  gboolean needs_type_label = stats_cluster_get_type_label(sc, type, &type_label);

  if (!sc->key.labels_len && !needs_type_label)
    return NULL;

  GString *serialized_labels = scratch_buffers_alloc();

  gboolean comma_needed = FALSE;
  for (gsize i = 0; i < sc->key.labels_len; ++i)
    {
      const StatsClusterLabel *label = &sc->key.labels[i];

      if (_is_str_empty(label->value))
        continue;

      if (comma_needed)
        g_string_append_c(serialized_labels, ',');
      else
        comma_needed = TRUE;

      _append_formatted_label(serialized_labels, label);
    }

  if (needs_type_label)
    {
      if (comma_needed)
        g_string_append_c(serialized_labels, ',');
      _append_formatted_label(serialized_labels, &type_label);
    }

  if (serialized_labels->len == 0)
    return NULL;

  return serialized_labels->str;
}

static GString *
_format_legacy(StatsCluster *sc, gint type, StatsCounterItem *counter)
{
  GString *record = scratch_buffers_alloc();
  GString *labels = scratch_buffers_alloc();

  gchar component[64];

  g_string_append_printf(record, PROMETHEUS_METRIC_PREFIX "%s",
                         stats_format_prometheus_sanitize_name(stats_cluster_get_component_name(sc, component, sizeof(component))));

  if (!sc->key.legacy.component || sc->key.legacy.component == SCS_GLOBAL)
    {
      if (!_is_str_empty(sc->key.legacy.id))
        g_string_append_printf(record, "_%s", stats_format_prometheus_sanitize_name(sc->key.legacy.id));
    }
  else
    {
      gboolean has_id = !_is_str_empty(sc->key.legacy.id);
      if (has_id)
        g_string_append_printf(labels, "%s=\"%s\"", "id", stats_format_prometheus_sanitize_label_value(sc->key.legacy.id));

      if (!_is_str_empty(sc->key.legacy.instance))
        {
          if (has_id)
            g_string_append_c(labels, ',');
          g_string_append_printf(labels, "%s=\"%s\"", "stat_instance",
                                 stats_format_prometheus_sanitize_label_value(sc->key.legacy.instance));
        }
    }

  const gchar *type_name = stats_cluster_get_type_name(sc, type);
  if (g_strcmp0(type_name, "value") != 0)
    g_string_append_printf(record, "_%s", stats_format_prometheus_sanitize_name(type_name));

  if (labels->len != 0)
    g_string_append_printf(record, "{%s}", labels->str);

  const gchar *metric_value = stats_format_prometheus_format_value(&sc->key, &sc->counter_group.counters[type]);
  g_string_append_printf(record, " %s\n", metric_value);

  return record;
}

GString *
stats_prometheus_format_counter(StatsCluster *sc, gint type, StatsCounterItem *counter)
{
  if (_is_timestamp(sc, type))
    return NULL;

  if (!sc->key.name)
    return _format_legacy(sc, type, counter);

  GString *record = scratch_buffers_alloc();
  g_string_append_printf(record, PROMETHEUS_METRIC_PREFIX "%s", stats_format_prometheus_sanitize_name(sc->key.name));

  const gchar *labels = _format_labels(sc, type);
  if (labels)
    g_string_append_printf(record, "{%s}", labels);

  const gchar *metric_value = stats_format_prometheus_format_value(&sc->key, &sc->counter_group.counters[type]);
  g_string_append_printf(record, " %s\n", metric_value);

  return record;
}

static void
stats_format_prometheus(StatsCluster *sc, gint type, StatsCounterItem *counter, gpointer user_data)
{
  gpointer *args = (gpointer *) user_data;
  StatsPrometheusRecordFunc process_record = (StatsPrometheusRecordFunc) args[0];
  gpointer process_record_arg = args[1];
  gboolean with_legacy = GPOINTER_TO_INT(args[2]);

  if (!sc->key.name && !with_legacy)
    return;

  if (stats_cluster_is_orphaned(sc))
    return;

  ScratchBuffersMarker marker;
  scratch_buffers_mark(&marker);

  GString *record = stats_prometheus_format_counter(sc, type, counter);
  if (!record)
    return;

  process_record(record->str, process_record_arg);
  scratch_buffers_reclaim_marked(marker);
}

void
stats_generate_prometheus(StatsPrometheusRecordFunc process_record, gpointer user_data, gboolean with_legacy,
                          gboolean *cancelled)
{
  gpointer format_prometheus_args[] = {process_record, user_data, GINT_TO_POINTER(with_legacy)};
  stats_lock();
  stats_foreach_counter(stats_format_prometheus, format_prometheus_args, cancelled);
  stats_unlock();
}
