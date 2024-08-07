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

#include "filterx-metrics-labels.h"
#include "filterx-eval.h"
#include "expr-literal.h"
#include "expr-literal-generator.h"
#include "object-string.h"
#include "object-dict-interface.h"
#include "metrics/dyn-metrics-cache.h"
#include "stats/stats-cluster-single.h"
#include "scratch-buffers.h"

#include <string.h>

typedef struct _FilterXMetricsLabel
{
  gchar *name;
  struct
  {
    FilterXExpr *expr;
    gchar *str;
  } value;
} FilterXMetricsLabel;

static gchar *
_format_str_obj(FilterXObject *obj)
{
  GString *str = scratch_buffers_alloc();
  return filterx_object_repr(obj, str) ? str->str : NULL;
}

static gchar *
_format_value_expr(FilterXExpr *expr)
{
  FilterXObject *obj = filterx_expr_eval_typed(expr);
  if (!obj)
    return NULL;

  gchar *result = _format_str_obj(obj);
  if (!result)
    filterx_eval_push_error("failed to format metrics label value, repr() failed", expr, obj);

  filterx_object_unref(obj);
  return result;
}

static gboolean
_label_format(FilterXMetricsLabel *self, StatsClusterLabel *sc_label)
{
  sc_label->name = self->name;

  if (self->value.str)
    {
      sc_label->value = self->value.str;
      return TRUE;
    }

  sc_label->value = _format_value_expr(self->value.expr);

  return !!sc_label->value;
}

static gboolean
_label_is_const(FilterXMetricsLabel *self)
{
  return !!self->value.str;
}

static gint
_label_cmp(gconstpointer a, gconstpointer b)
{
  const FilterXMetricsLabel *lhs = *(const FilterXMetricsLabel **) a;
  const FilterXMetricsLabel *rhs = *(const FilterXMetricsLabel **) b;

  return strcmp(lhs->name, rhs->name);
}

static void
_label_free(FilterXMetricsLabel *self)
{
  g_free(self->name);
  filterx_expr_unref(self->value.expr);
  g_free(self->value.str);

  g_free(self);
}

static gchar *
_init_label_name(FilterXExpr *name)
{
  if (!filterx_expr_is_literal(name))
    {
      filterx_eval_push_error("failed to initialize metrics label name, name must be a string literal", name, NULL);
      return NULL;
    }

  FilterXObject *obj = filterx_expr_eval_typed(name);
  gchar *str = g_strdup(filterx_string_get_value(obj, NULL));
  if (!str)
    filterx_eval_push_error_info("failed to initialize metrics label name, name must be a string literal", name,
                                 g_strdup_printf("got %s instead", obj->type->name), TRUE);

  filterx_object_unref(obj);
  return str;
}

static gboolean
_init_label_value(FilterXMetricsLabel *self, FilterXExpr *value)
{
  if (!filterx_expr_is_literal(value))
    {
      self->value.expr = filterx_expr_ref(value);
      return TRUE;
    }

  ScratchBuffersMarker marker;
  scratch_buffers_mark(&marker);
  self->value.str = g_strdup(_format_value_expr(value));
  scratch_buffers_reclaim_marked(marker);

  return !!self->value.str;
}

static FilterXMetricsLabel *
_label_new(FilterXExpr *name, FilterXExpr *value)
{
  FilterXMetricsLabel *self = g_new0(FilterXMetricsLabel, 1);

  self->name = _init_label_name(name);
  if (!self->name)
    goto error;

  if (!_init_label_value(self, value))
    goto error;

  return self;

error:
  _label_free(self);
  return NULL;
}


struct _FilterXMetricsLabels
{
  FilterXExpr *expr;
  GPtrArray *literal_labels;
  gboolean is_const;
};

static void
_check_label_is_const(gpointer data, gpointer user_data)
{
  FilterXMetricsLabel *label = (FilterXMetricsLabel *) data;
  gboolean *is_const = (gboolean *) user_data;

  if (!(*is_const))
    return;

  if (!_label_is_const(label))
    *is_const = FALSE;
}

gboolean
filterx_metrics_labels_is_const(FilterXMetricsLabels *self)
{
  return self->is_const;
}

static gboolean
_format_dict_elem_to_cache(FilterXObject *key, FilterXObject *value, gpointer user_data)
{
  DynMetricsStore *cache = (DynMetricsStore *) user_data;

  const gchar *name_str = _format_str_obj(key);
  if (!name_str)
    {
      filterx_eval_push_error_info("failed to format label name, name must be string", NULL,
                                   g_strdup_printf("got %s instead", key->type->name), TRUE);
      return FALSE;
    }

  const gchar *value_str = _format_str_obj(value);
  if (!value_str)
    {
      filterx_eval_push_error_info("failed to format label value, object does not support string casting", NULL,
                                   "", FALSE);
      return FALSE;
    }

  StatsClusterLabel *label = dyn_metrics_store_cache_label(cache);
  label->name = name_str;
  label->value = value_str;

  return TRUE;
}

static gboolean
_format_expr_to_cache(FilterXExpr *expr, DynMetricsStore *cache)
{
  FilterXObject *obj = filterx_expr_eval_typed(expr);
  if (!obj)
    return FALSE;

  gboolean success = FALSE;

  if (!filterx_object_is_type(obj, &FILTERX_TYPE_NAME(dict)))
    {
      filterx_eval_push_error_info("failed to format metrics labels, labels must be a dict", expr,
                                   g_strdup_printf("got %s instead", obj->type->name), TRUE);
      goto exit;
    }

  success = filterx_dict_iter(obj, _format_dict_elem_to_cache, cache);
  if (!success)
    goto exit;

  dyn_metrics_store_sort_cached_labels(cache);

exit:
  filterx_object_unref(obj);
  return success;
}

static void
_format_label_to_cache(gpointer data, gpointer user_data)
{
  FilterXMetricsLabel *label = (FilterXMetricsLabel *) data;
  gboolean *success = ((gpointer *) user_data)[0];
  DynMetricsStore *cache = ((gpointer *) user_data)[1];

  if (!(*success))
    return;

  *success = _label_format(label, dyn_metrics_store_cache_label(cache));
}

gboolean
filterx_metrics_labels_format(FilterXMetricsLabels *self, StatsClusterLabel **labels, gsize *len)
{
  DynMetricsStore *cache = dyn_metrics_cache();

  dyn_metrics_store_reset_labels_cache(cache);

  gboolean success;
  if (self->expr)
    {
      success = _format_expr_to_cache(self->expr, cache);
    }
  else
    {
      success = TRUE;
      gpointer user_data[] = { &success, cache };
      g_ptr_array_foreach(self->literal_labels, _format_label_to_cache, user_data);
    }

  if (!success)
    return FALSE;

  *labels = dyn_metrics_store_get_cached_labels(cache);
  *len = dyn_metrics_store_get_cached_labels_len(cache);
  return TRUE;
}

void
filterx_metrics_labels_free(FilterXMetricsLabels *self)
{
  filterx_expr_unref(self->expr);
  if (self->literal_labels)
    g_ptr_array_unref(self->literal_labels);

  g_free(self);
}

static gboolean
_add_literal_label(FilterXExpr *key, FilterXExpr *value, gpointer user_data)
{
  GPtrArray *literal_labels = (GPtrArray *) user_data;

  FilterXMetricsLabel *label = _label_new(key, value);
  if (!label)
    return FALSE;

  g_ptr_array_add(literal_labels, label);
  return TRUE;
}

static GPtrArray *
_init_literal_labels(FilterXExpr *labels_expr)
{
  GPtrArray *literal_labels = g_ptr_array_new_with_free_func((GDestroyNotify) _label_free);

  if (!filterx_literal_dict_generator_foreach(labels_expr, _add_literal_label, literal_labels))
    {
      g_ptr_array_unref(literal_labels);
      literal_labels = NULL;
      goto exit;
    }

  g_ptr_array_sort(literal_labels, _label_cmp);

exit:
  return literal_labels;
}

static gboolean
_init_labels(FilterXMetricsLabels *self, FilterXExpr *labels)
{
  if (!labels)
    {
      self->literal_labels = g_ptr_array_new_with_free_func((GDestroyNotify) _label_free);
      return TRUE;
    }

  if (filterx_expr_is_literal_dict_generator(labels))
    {
      self->literal_labels = _init_literal_labels(labels);
      return !!self->literal_labels;
    }

  self->expr = filterx_expr_ref(labels);
  return TRUE;
}

static gboolean
_calculate_constness(FilterXMetricsLabels *self)
{
  if (self->expr)
    return FALSE;

  gboolean is_const = TRUE;
  g_ptr_array_foreach(self->literal_labels, _check_label_is_const, &is_const);
  return is_const;
}

FilterXMetricsLabels *
filterx_metrics_labels_new(FilterXExpr *labels)
{
  FilterXMetricsLabels *self = g_new0(FilterXMetricsLabels, 1);

  if (!_init_labels(self, labels))
    {
      filterx_metrics_labels_free(self);
      return NULL;
    }

  self->is_const = _calculate_constness(self);

  return self;
}
