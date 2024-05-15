/*
 * Copyright (c) 2023 Balázs Scheidler <balazs.scheidler@axoflow.com>
 * Copyright (c) 2023 shifter
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

#include "filterx/expr-function.h"
#include "filterx/filterx-grammar.h"
#include "filterx/filterx-globals.h"
#include "filterx/filterx-eval.h"
#include "filterx/expr-literal.h"
#include "filterx/object-string.h"
#include "filterx/object-null.h"
#include "plugin.h"
#include "cfg.h"

GQuark
filterx_function_error_quark(void)
{
  return g_quark_from_static_string("filterx-function-error-quark");
}

typedef struct _FilterXSimpleFunction
{
  FilterXFunction super;
  FilterXFunctionArgs *args;
  FilterXSimpleFunctionProto function_proto;
} FilterXSimpleFunction;

static GPtrArray *
_simple_function_eval_args(FilterXFunctionArgs *args)
{
  guint64 len = filterx_function_args_len(args);
  if (len == 0)
    return NULL;

  GPtrArray *res = g_ptr_array_new_full(len, (GDestroyNotify) filterx_object_unref);

  for (guint64 i = 0; i < len; i++)
    {
      FilterXObject *obj = filterx_function_args_get_object(args, i);
      if (obj == NULL)
        goto error;

      g_ptr_array_add(res, obj);
    }

  return res;

error:
  g_ptr_array_free(res, TRUE);
  return NULL;
}

static FilterXObject *
_simple_eval(FilterXExpr *s)
{
  FilterXSimpleFunction *self = (FilterXSimpleFunction *) s;

  GPtrArray *args = _simple_function_eval_args(self->args);

  FilterXSimpleFunctionProto f = self->function_proto;

  g_assert(f != NULL);
  FilterXObject *res = f(args);

  if (args != NULL)
    g_ptr_array_free(args, TRUE);
  if (!res)
    filterx_eval_push_error("Function call failed", s, NULL);

  return res;
}

static void
_simple_free(FilterXExpr *s)
{
  FilterXSimpleFunction *self = (FilterXSimpleFunction *) s;
  filterx_function_args_free(self->args);
  filterx_function_free_method(&self->super);
}

FilterXExpr *
filterx_simple_function_new(const gchar *function_name, FilterXFunctionArgs *args,
                            FilterXSimpleFunctionProto function_proto)
{
  FilterXSimpleFunction *self = g_new0(FilterXSimpleFunction, 1);

  filterx_function_init_instance(&self->super, function_name);
  self->super.super.eval = _simple_eval;
  self->super.super.free_fn = _simple_free;
  self->args = args;
  self->function_proto = function_proto;

  return &self->super.super;
}

void
filterx_function_free_method(FilterXFunction *s)
{
  g_free(s->function_name);
  filterx_expr_free_method(&s->super);
}

static void
_function_free(FilterXExpr *s)
{
  FilterXFunction *self = (FilterXFunction *) s;
  filterx_function_free_method(self);
}

void
filterx_function_init_instance(FilterXFunction *s, const gchar *function_name)
{
  filterx_expr_init_instance(&s->super);
  s->function_name = g_strdup(function_name);
  s->super.free_fn = _function_free;
}

struct _FilterXFunctionArgs
{
  GList *positional_args;
  GHashTable *named_args;
};

/* Takes reference of value */
FilterXFunctionArg *
filterx_function_arg_new(const gchar *name, FilterXExpr *value)
{
  FilterXFunctionArg *self = g_new0(FilterXFunctionArg, 1);

  self->name = g_strdup(name);
  self->value = value;

  return self;
}

static void
filterx_function_arg_free(FilterXFunctionArg *self)
{
  g_free(self->name);
  filterx_expr_unref(self->value);
  g_free(self);
}

/* Takes reference of positional_args. */
FilterXFunctionArgs *
filterx_function_args_new(GList *args, GError **error)
{
  FilterXFunctionArgs *self = g_new0(FilterXFunctionArgs, 1);

  self->named_args = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) filterx_expr_unref);

  gboolean has_named = FALSE;
  for (GList *elem = args; elem; elem = elem->next)
    {
      FilterXFunctionArg *arg = (FilterXFunctionArg *) elem->data;
      if (!arg->name)
        {
          if (has_named)
            {
              g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                          "cannot set positional argument after a named argument");
              filterx_function_args_free(self);
              self = NULL;
              goto exit;
            }
          self->positional_args = g_list_append(self->positional_args, filterx_expr_ref(arg->value));
        }
      else
        {
          g_hash_table_insert(self->named_args, g_strdup(arg->name), filterx_expr_ref(arg->value));
          has_named = TRUE;
        }
    }

exit:
  g_list_free_full(args, (GDestroyNotify) filterx_function_arg_free);
  return self;
}

guint64
filterx_function_args_len(FilterXFunctionArgs *self)
{
  return g_list_length(self->positional_args);
}

FilterXExpr *
filterx_function_args_get_expr(FilterXFunctionArgs *self, guint64 index)
{
  if (g_list_length(self->positional_args) <= index)
    return NULL;

  return filterx_expr_ref((FilterXExpr *) g_list_nth_data(self->positional_args, index));
}

FilterXObject *
filterx_function_args_get_object(FilterXFunctionArgs *self, guint64 index)
{
  FilterXExpr *expr = filterx_function_args_get_expr(self, index);
  if (!expr)
    return NULL;

  FilterXObject *obj = filterx_expr_eval(expr);
  filterx_expr_unref(expr);
  return obj;
}

static const gchar *
_get_literal_string_from_expr(FilterXExpr *expr, gsize *len)
{
  FilterXObject *obj = NULL;
  const gchar *str = NULL;

  if (!filterx_expr_is_literal(expr))
    goto error;

  obj = filterx_expr_eval(expr);
  if (!obj)
    goto error;

  str = filterx_string_get_value(obj, len);

  /*
   * We can unref both obj and expr, the underlying string will be kept alive as long as the literal expr is alive,
   * which is kept alive in either in the positional or named args container.
   */

error:
  filterx_object_unref(obj);
  return str;
}

const gchar *
filterx_function_args_get_literal_string(FilterXFunctionArgs *self, guint64 index, gsize *len)
{
  FilterXExpr *expr = filterx_function_args_get_expr(self, index);
  if (!expr)
    return NULL;

  const gchar *str = _get_literal_string_from_expr(expr, len);

  filterx_expr_unref(expr);
  return str;
}

gboolean
filterx_function_args_is_literal_null(FilterXFunctionArgs *self, guint64 index)
{
  gboolean is_literal_null = FALSE;

  FilterXExpr *expr = filterx_function_args_get_expr(self, index);
  if (!expr)
    goto error;

  if (!filterx_expr_is_literal(expr))
    goto error;

  FilterXObject *obj = filterx_expr_eval(expr);
  if (!obj)
    goto error;

  is_literal_null = filterx_object_is_type(obj, &FILTERX_TYPE_NAME(null));
  filterx_object_unref(obj);

error:
  filterx_expr_unref(expr);
  return is_literal_null;
}

FilterXExpr *
filterx_function_args_get_named_expr(FilterXFunctionArgs *self, const gchar *name)
{
  return filterx_expr_ref((FilterXExpr *) g_hash_table_lookup(self->named_args, name));
}

FilterXObject *
filterx_function_args_get_named_object(FilterXFunctionArgs *self, const gchar *name, gboolean *exists)
{
  FilterXExpr *expr = filterx_function_args_get_named_expr(self, name);
  *exists = !!expr;
  if (!expr)
    return NULL;

  FilterXObject *obj = filterx_expr_eval(expr);
  filterx_expr_unref(expr);
  return obj;
}

const gchar *
filterx_function_args_get_named_literal_string(FilterXFunctionArgs *self, const gchar *name, gsize *len,
                                               gboolean *exists)
{
  FilterXExpr *expr = filterx_function_args_get_named_expr(self, name);
  *exists = !!expr;
  if (!expr)
    return NULL;

  const gchar *str = _get_literal_string_from_expr(expr, len);

  filterx_expr_unref(expr);
  return str;
}

void
filterx_function_args_free(FilterXFunctionArgs *self)
{
  g_list_free_full(self->positional_args, (GDestroyNotify) filterx_expr_unref);
  g_hash_table_unref(self->named_args);
  g_free(self);
}


static FilterXExpr *
_lookup_simple_function(GlobalConfig *cfg, const gchar *function_name, FilterXFunctionArgs *args)
{
  // Checking filterx builtin functions first
  FilterXSimpleFunctionProto f = filterx_builtin_simple_function_lookup(function_name);
  if (f != NULL)
    {
      return filterx_simple_function_new(function_name, args, f);
    }

  // fallback to plugin lookup
  Plugin *p = cfg_find_plugin(cfg, LL_CONTEXT_FILTERX_SIMPLE_FUNC, function_name);

  if (p == NULL)
    return NULL;

  f = plugin_construct(p);
  if (f == NULL)
    return NULL;

  return filterx_simple_function_new(function_name, args, f);
}

static FilterXExpr *
_lookup_function(GlobalConfig *cfg, const gchar *function_name, FilterXFunctionArgs *args, GError **error)
{
  // Checking filterx builtin functions first
  FilterXFunctionCtor ctor = filterx_builtin_function_ctor_lookup(function_name);

  if (!ctor)
    {
      // fallback to plugin lookup
      Plugin *p = cfg_find_plugin(cfg, LL_CONTEXT_FILTERX_FUNC, function_name);
      if (!p)
        return NULL;
      ctor = plugin_construct(p);
    }

  if (!ctor)
    return NULL;

  FilterXFunction *func_expr = ctor(function_name, args, error);
  if (!func_expr)
    return NULL;
  return &func_expr->super;
}

/* NOTE: takes the reference of "args_list" */
FilterXExpr *
filterx_function_lookup(GlobalConfig *cfg, const gchar *function_name, GList *args_list, GError **error)
{
  FilterXFunctionArgs *args = filterx_function_args_new(args_list, error);
  if (!args)
    return NULL;

  FilterXExpr *expr = _lookup_simple_function(cfg, function_name, args);
  if (expr)
    return expr;

  expr = _lookup_function(cfg, function_name, args, error);
  if (expr)
    return expr;

  if (!(*error))
    g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_FUNCTION_NOT_FOUND, "function not found");
  return NULL;
}
