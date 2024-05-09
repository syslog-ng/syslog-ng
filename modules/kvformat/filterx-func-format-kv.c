/*
 * Copyright (c) 2024 Attila Szakacs
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

#include "filterx-func-format-kv.h"
#include "filterx/expr-literal.h"
#include "filterx/object-string.h"
#include "filterx/object-null.h"
#include "filterx/object-dict-interface.h"
#include "filterx/object-list-interface.h"
#include "filterx/filterx-eval.h"

#include "scratch-buffers.h"

#define FILTERX_FUNC_FORMAT_KV_USAGE "Usage: format_kv(kvs_dict, optional_value_separator, optional_pair_separator)"

typedef struct FilterXFunctionFormatKV_
{
  FilterXFunction super;
  FilterXExpr *kvs;
  gchar value_separator;
  gchar *pair_separator;
} FilterXFunctionFormatKV;

static gboolean
_append_kv_to_buffer(FilterXObject *key, FilterXObject *value, gpointer user_data)
{
  FilterXFunctionFormatKV *self = ((gpointer *) user_data)[0];
  GString *buffer = ((gpointer *) user_data)[1];

  if (filterx_object_is_type(value, &FILTERX_TYPE_NAME(dict)) ||
      filterx_object_is_type(value, &FILTERX_TYPE_NAME(list)))
    {
      msg_debug("FilterX: format_kv(): skipping object, type not supported",
                evt_tag_str("type", value->type->name));
      return TRUE;
    }

  if (buffer->len)
    g_string_append(buffer, self->pair_separator);

  if (!filterx_object_repr_append(key, buffer))
    return FALSE;

  g_string_append_c(buffer, self->value_separator);

  if (!filterx_object_repr_append(value, buffer))
    return FALSE;

  return TRUE;
}

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXFunctionFormatKV *self = (FilterXFunctionFormatKV *) s;

  FilterXObject *kvs = filterx_expr_eval_typed(self->kvs);
  if (!kvs)
    {
      filterx_eval_push_error("Failed to evaluate kvs_dict. " FILTERX_FUNC_FORMAT_KV_USAGE, s, NULL);
      return NULL;
    }

  if (!filterx_object_is_type(kvs, &FILTERX_TYPE_NAME(dict)))
    {
      filterx_eval_push_error("kvs_dict must be a dict. " FILTERX_FUNC_FORMAT_KV_USAGE, s, kvs);
      filterx_object_unref(kvs);
      return NULL;
    }

  GString *formatted = scratch_buffers_alloc();
  gpointer user_data[] = { self, formatted };
  gboolean success = filterx_dict_iter(kvs, _append_kv_to_buffer, user_data);

  filterx_object_unref(kvs);
  return success ? filterx_string_new(formatted->str, formatted->len) : NULL;
}

static void
_free(FilterXExpr *s)
{
  FilterXFunctionFormatKV *self = (FilterXFunctionFormatKV *) s;

  filterx_expr_unref(self->kvs);
  g_free(self->pair_separator);
  filterx_function_free_method(&self->super);
}

static gboolean
_extract_value_separator_arg(FilterXFunctionFormatKV *self, FilterXExpr *value_separator_expr, GError **error)
{
  FilterXObject *value_separator_obj = filterx_expr_eval_typed(value_separator_expr);

  if (!value_separator_obj ||
      !(filterx_object_is_type(value_separator_obj, &FILTERX_TYPE_NAME(null)) ||
        filterx_object_is_type(value_separator_obj, &FILTERX_TYPE_NAME(string))) ||
      !filterx_expr_is_literal(value_separator_expr))
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "value_separator must be a string literal or null. " FILTERX_FUNC_FORMAT_KV_USAGE);
      filterx_object_unref(value_separator_obj);
      return FALSE;
    }

  if (filterx_object_is_type(value_separator_obj, &FILTERX_TYPE_NAME(null)))
    {
      filterx_object_unref(value_separator_obj);
      return TRUE;
    }

  gsize value_separator_len;
  const gchar *value_separator_str = filterx_string_get_value(value_separator_obj, &value_separator_len);
  if (value_separator_len != 1)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "value_separator must be a single character. " FILTERX_FUNC_FORMAT_KV_USAGE);
      filterx_object_unref(value_separator_obj);
      return FALSE;
    }

  self->value_separator = value_separator_str[0];
  filterx_object_unref(value_separator_obj);
  return TRUE;
}

static gboolean
_extract_pair_separator_arg(FilterXFunctionFormatKV *self, FilterXExpr *pair_separator_expr, GError **error)
{
  FilterXObject *pair_separator_obj = filterx_expr_eval_typed(pair_separator_expr);

  if (!pair_separator_obj ||
      !(filterx_object_is_type(pair_separator_obj, &FILTERX_TYPE_NAME(null)) ||
        filterx_object_is_type(pair_separator_obj, &FILTERX_TYPE_NAME(string))) ||
      !filterx_expr_is_literal(pair_separator_expr))
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "pair_separator must be a string literal or null. " FILTERX_FUNC_FORMAT_KV_USAGE);
      filterx_object_unref(pair_separator_obj);
      return FALSE;
    }

  if (filterx_object_is_type(pair_separator_obj, &FILTERX_TYPE_NAME(null)))
    {
      filterx_object_unref(pair_separator_obj);
      return TRUE;
    }

  gsize pair_separator_len;
  const gchar *pair_separator_str = filterx_string_get_value(pair_separator_obj, &pair_separator_len);
  if (pair_separator_len == 0)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "pair_separator must be non-zero length. " FILTERX_FUNC_FORMAT_KV_USAGE);
      filterx_object_unref(pair_separator_obj);
      return FALSE;
    }

  g_free(self->pair_separator);
  self->pair_separator = g_strdup(pair_separator_str);
  filterx_object_unref(pair_separator_obj);
  return TRUE;
}

static gboolean
_extract_arguments(FilterXFunctionFormatKV *self, GList *argument_expressions, GError **error)
{
  gsize arguments_len = argument_expressions ? g_list_length(argument_expressions) : 0;
  if (arguments_len < 1 || arguments_len > 3)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "invalid number of arguments. " FILTERX_FUNC_FORMAT_KV_USAGE);
      return FALSE;
    }

  self->kvs = filterx_expr_ref((FilterXExpr *) argument_expressions->data);
  if (!self->kvs)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "kvs_dict must be set. " FILTERX_FUNC_FORMAT_KV_USAGE);
      return FALSE;
    }

  if (!argument_expressions->next)
    return TRUE;

  FilterXExpr *value_separator_expr = (FilterXExpr *) argument_expressions->next->data;
  if (value_separator_expr && !_extract_value_separator_arg(self, value_separator_expr, error))
    return FALSE;

  if (!argument_expressions->next->next)
    return TRUE;

  FilterXExpr *pair_separator_expr = (FilterXExpr *) argument_expressions->next->next->data;
  if (pair_separator_expr && !_extract_pair_separator_arg(self, pair_separator_expr, error))
    return FALSE;

  g_assert(!argument_expressions->next->next->next);

  return TRUE;
}

FilterXFunction *
filterx_function_format_kv_new(const gchar *function_name, GList *argument_expressions, GError **error)
{
  FilterXFunctionFormatKV *self = g_new0(FilterXFunctionFormatKV, 1);
  filterx_function_init_instance(&self->super, function_name);

  self->super.super.eval = _eval;
  self->super.super.free_fn = _free;
  self->value_separator = '=';
  self->pair_separator = g_strdup(", ");

  if (!_extract_arguments(self, argument_expressions, error))
    goto error;

  g_list_free_full(argument_expressions, (GDestroyNotify) filterx_expr_unref);
  return &self->super;

error:
  g_list_free_full(argument_expressions, (GDestroyNotify) filterx_expr_unref);
  filterx_expr_unref(&self->super.super);
  return NULL;
}

gpointer
filterx_function_construct_format_kv(Plugin *self)
{
  return (gpointer) filterx_function_format_kv_new;
}
