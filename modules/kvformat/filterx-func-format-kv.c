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
#include "utf8utils.h"

#define FILTERX_FUNC_FORMAT_KV_USAGE "Usage: format_kv(kvs_dict, value_separator=\"=\", pair_separator=\", \")"

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

  gsize len_before_value = buffer->len;
  if (!filterx_object_repr_append(value, buffer))
    return FALSE;

  /* TODO: make the characters here configurable. */
  if (memchr(buffer->str + len_before_value, ' ', buffer->len - len_before_value) != NULL)
    {
      ScratchBuffersMarker marker;
      GString *value_buffer = scratch_buffers_alloc_and_mark(&marker);

      g_string_assign(value_buffer, buffer->str + len_before_value);
      g_string_truncate(buffer, len_before_value);
      g_string_append_c(buffer, '"');
      append_unsafe_utf8_as_escaped_binary(buffer, value_buffer->str, value_buffer->len, "\"");
      g_string_append_c(buffer, '"');

      scratch_buffers_reclaim_marked(marker);
    }

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
_extract_value_separator_arg(FilterXFunctionFormatKV *self, FilterXFunctionArgs *args, GError **error)
{
  gboolean exists;
  gsize value_separator_len;
  const gchar *value_separator = filterx_function_args_get_named_literal_string(args, "value_separator",
                                 &value_separator_len, &exists);
  if (!exists)
    return TRUE;

  if (!value_separator)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "value_separator must be a string literal. " FILTERX_FUNC_FORMAT_KV_USAGE);
      return FALSE;
    }

  if (value_separator_len != 1)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "value_separator must be a single character. " FILTERX_FUNC_FORMAT_KV_USAGE);
      return FALSE;
    }

  self->value_separator = value_separator[0];
  return TRUE;
}

static gboolean
_extract_pair_separator_arg(FilterXFunctionFormatKV *self, FilterXFunctionArgs *args, GError **error)
{
  gboolean exists;
  gsize pair_separator_len;
  const gchar *pair_separator = filterx_function_args_get_named_literal_string(args, "pair_separator",
                                &pair_separator_len, &exists);
  if (!exists)
    return TRUE;

  if (!pair_separator)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "pair_separator must be a string literal. " FILTERX_FUNC_FORMAT_KV_USAGE);
      return FALSE;
    }

  if (pair_separator_len == 0)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "pair_separator must be non-zero length. " FILTERX_FUNC_FORMAT_KV_USAGE);
      return FALSE;
    }

  g_free(self->pair_separator);
  self->pair_separator = g_strdup(pair_separator);
  return TRUE;
}

static gboolean
_extract_arguments(FilterXFunctionFormatKV *self, FilterXFunctionArgs *args, GError **error)
{
  gsize args_len = filterx_function_args_len(args);
  if (args_len != 1)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "invalid number of arguments. " FILTERX_FUNC_FORMAT_KV_USAGE);
      return FALSE;
    }

  self->kvs = filterx_function_args_get_expr(args, 0);
  if (!self->kvs)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "kvs_dict must be set. " FILTERX_FUNC_FORMAT_KV_USAGE);
      return FALSE;
    }

  if (!_extract_value_separator_arg(self, args, error))
    return FALSE;

  if (!_extract_pair_separator_arg(self, args, error))
    return FALSE;

  return TRUE;
}

FilterXFunction *
filterx_function_format_kv_new(const gchar *function_name, FilterXFunctionArgs *args, GError **error)
{
  FilterXFunctionFormatKV *self = g_new0(FilterXFunctionFormatKV, 1);
  filterx_function_init_instance(&self->super, function_name);

  self->super.super.eval = _eval;
  self->super.super.free_fn = _free;
  self->value_separator = '=';
  self->pair_separator = g_strdup(", ");

  if (!_extract_arguments(self, args, error))
    goto error;

  filterx_function_args_free(args);
  return &self->super;

error:
  filterx_function_args_free(args);
  filterx_expr_unref(&self->super.super);
  return NULL;
}

gpointer
filterx_function_construct_format_kv(Plugin *self)
{
  return (gpointer) filterx_function_format_kv_new;
}
