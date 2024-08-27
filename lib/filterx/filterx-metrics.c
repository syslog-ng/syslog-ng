/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
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

#include "filterx-metrics.h"
#include "filterx-metrics-labels.h"
#include "expr-literal.h"
#include "object-extractor.h"
#include "object-string.h"
#include "filterx-eval.h"
#include "stats/stats-cluster-single.h"
#include "stats/stats-registry.h"
#include "metrics/dyn-metrics-cache.h"
#include "scratch-buffers.h"
#include "atomic.h"

struct _FilterXMetrics
{
  struct
  {
    FilterXExpr *expr;
    gchar *str;
  } key;

  FilterXMetricsLabels *labels;
  gint level;

  GAtomicCounter is_optimized;
  StatsCluster *const_cluster;
};

gboolean
filterx_metrics_is_enabled(FilterXMetrics *self)
{
  return stats_check_level(self->level);
}

static const gchar *
_format_sck_name(FilterXMetrics *self)
{
  if (self->key.str)
    return self->key.str;

  FilterXObject *key_obj = filterx_expr_eval(self->key.expr);
  if (!key_obj)
    return NULL;

  gsize len;
  const gchar *name;
  if (!filterx_object_extract_string(key_obj, &name, &len) || len == 0)
    {
      filterx_eval_push_error("failed to format metrics key: key must be a non-empty string", self->key.expr, key_obj);
      goto exit;
    }

  GString *name_buffer = scratch_buffers_alloc();
  g_string_append_len(name_buffer, name, (gssize) len);
  name = name_buffer->str;

exit:
  filterx_object_unref(key_obj);
  return name;
}

static gboolean
_format_sck(FilterXMetrics *self, StatsClusterKey *sck)
{
  const gchar *name = _format_sck_name(self);
  if (!name)
    return FALSE;

  StatsClusterLabel *labels;
  gsize labels_len;
  if (!filterx_metrics_labels_format(self->labels, &labels, &labels_len))
    return FALSE;

  stats_cluster_single_key_set(sck, name, labels, labels_len);
  return TRUE;
}

static gboolean
_is_const(FilterXMetrics *self)
{
  return !self->key.expr && (!self->labels || filterx_metrics_labels_is_const(self->labels));
}

static void
_optimize(FilterXMetrics *self)
{
  stats_lock();

  if (g_atomic_counter_get(&self->is_optimized))
    goto exit;

  if (!self->key.str || !filterx_metrics_labels_is_const(self->labels))
    goto exit;

  ScratchBuffersMarker marker;
  scratch_buffers_mark(&marker);

  StatsClusterKey sck;
  if (!_format_sck(self, &sck))
    {
      msg_debug("FilterX: Failed to optimize metrics, continuing unoptimized");
      scratch_buffers_reclaim_marked(marker);
      goto exit;
    }

  StatsCounterItem *counter;
  self->const_cluster = stats_register_dynamic_counter(self->level, &sck, SC_TYPE_SINGLE_VALUE, &counter);

  scratch_buffers_reclaim_marked(marker);

  g_free(self->key.str);
  self->key.str = NULL;

  filterx_metrics_labels_free(self->labels);
  self->labels = NULL;

exit:
  g_atomic_counter_set(&self->is_optimized, TRUE);
  stats_unlock();
}

gboolean
filterx_metrics_get_stats_counter(FilterXMetrics *self, StatsCounterItem **counter)
{
  if (!filterx_metrics_is_enabled(self))
    {
      *counter = NULL;
      return TRUE;
    }

  /*
   * We need to delay the optimization to the first get() call,
   * as we don't have stats options when FilterXExprs are being created.
   */
  if (!g_atomic_counter_get(&self->is_optimized))
    _optimize(self);

  if (_is_const(self))
    {
      *counter = stats_cluster_single_get_counter(self->const_cluster);
      return TRUE;
    }

  gboolean success = FALSE;

  ScratchBuffersMarker marker;
  scratch_buffers_mark(&marker);

  StatsClusterKey sck;
  if (!_format_sck(self, &sck))
    goto exit;

  *counter = dyn_metrics_store_retrieve_counter(dyn_metrics_cache(), &sck, self->level);
  success = TRUE;

exit:
  scratch_buffers_reclaim_marked(marker);
  return success;
}

void
filterx_metrics_free(FilterXMetrics *self)
{
  filterx_expr_unref(self->key.expr);
  g_free(self->key.str);

  if (self->labels)
    filterx_metrics_labels_free(self->labels);

  stats_lock();
  {
    StatsCounterItem *counter = stats_cluster_single_get_counter(self->const_cluster);
    stats_unregister_dynamic_counter(self->const_cluster, SC_TYPE_SINGLE_VALUE, &counter);
  }
  stats_unlock();

  g_free(self);
}

static gboolean
_init_key(FilterXMetrics *self, FilterXExpr *key)
{
  if (!filterx_expr_is_literal(key))
    {
      self->key.expr = filterx_expr_ref(key);
      return TRUE;
    }

  FilterXObject *key_obj = filterx_expr_eval_typed(key);
  if (!filterx_object_is_type(key_obj, &FILTERX_TYPE_NAME(string)))
    {
      filterx_eval_push_error_info("failed to init metrics key, key must be a string", key,
                                   g_strdup_printf("got %s instread", key_obj->type->name), TRUE);
      filterx_object_unref(key_obj);
      return FALSE;
    }

  /* There are no literal message values, so we don't need to call extract_string() here. */
  self->key.str = g_strdup(filterx_string_get_value(key_obj, NULL));
  return TRUE;
}

static gboolean
_init_labels(FilterXMetrics *self, FilterXExpr *labels)
{
  self->labels = filterx_metrics_labels_new(labels);
  return !!self->labels;
}

FilterXMetrics *
filterx_metrics_new(gint level, FilterXExpr *key, FilterXExpr *labels)
{
  FilterXMetrics *self = g_new0(FilterXMetrics, 1);

  g_assert(key);

  self->level = level;

  if (!_init_key(self, key))
    goto error;

  if (!_init_labels(self, labels))
    goto error;

  g_atomic_counter_set(&self->is_optimized, FALSE);

  return self;

error:
  filterx_metrics_free(self);
  return NULL;
}
