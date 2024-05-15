/*
 * Copyright (c) 2024 shifter
 * Copyright (c) 2024 Attila Szakacs
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

#include "filterx/func-istype.h"
#include "filterx/object-string.h"
#include "filterx/object-primitive.h"
#include "filterx/expr-literal.h"
#include "filterx/filterx-eval.h"
#include "filterx/filterx-globals.h"

#define FILTERX_FUNC_ISTYPE_USAGE "Usage: istype(object, type_str)"

typedef struct FilterXFunctionIsType_
{
  FilterXFunction super;
  FilterXExpr *object_expr;
  FilterXType *type;
} FilterXFunctionIsType;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXFunctionIsType *self = (FilterXFunctionIsType *) s;

  FilterXObject *object_expr = filterx_expr_eval(self->object_expr);
  if (!object_expr)
    {
      filterx_eval_push_error("Failed to evaluate first argument. " FILTERX_FUNC_ISTYPE_USAGE, s, NULL);
      return NULL;
    }

  gboolean result = filterx_object_is_type(object_expr, self->type);
  filterx_object_unref(object_expr);
  return filterx_boolean_new(result);
}

static void
_free(FilterXExpr *s)
{
  FilterXFunctionIsType *self = (FilterXFunctionIsType *) s;
  filterx_expr_unref(self->object_expr);
  filterx_function_free_method(&self->super);
}

static FilterXType *
_extract_type(FilterXFunctionArgs *args, GError **error)
{
  const gchar *type_str = filterx_function_args_get_literal_string(args, 1, NULL);
  if (!type_str)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "argument must be string literal: type. " FILTERX_FUNC_ISTYPE_USAGE);
      return NULL;
    }

  FilterXType *type = filterx_type_lookup(type_str);
  if (!type)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "unknown type: %s. " FILTERX_FUNC_ISTYPE_USAGE, type_str);
      return NULL;
    }

  return type;
}

static FilterXExpr *
_extract_object_expr(FilterXFunctionArgs *args, GError **error)
{
  FilterXExpr *object_expr = filterx_function_args_get_expr(args, 0);
  if (!object_expr)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "argument must be set: object. " FILTERX_FUNC_ISTYPE_USAGE);
      return NULL;
    }

  return object_expr;
}

static gboolean
_extract_args(FilterXFunctionIsType *self, FilterXFunctionArgs *args, GError **error)
{
  if (filterx_function_args_len(args) != 2)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "invalid number of arguments. " FILTERX_FUNC_ISTYPE_USAGE);
      return FALSE;
    }

  self->object_expr = _extract_object_expr(args, error);
  if (!self->object_expr)
    return FALSE;

  self->type = _extract_type(args, error);
  if (!self->type)
    return FALSE;

  return TRUE;
}

FilterXFunction *
filterx_function_istype_new(const gchar *function_name, FilterXFunctionArgs *args, GError **error)
{
  FilterXFunctionIsType *self = g_new0(FilterXFunctionIsType, 1);
  filterx_function_init_instance(&self->super, function_name);
  self->super.super.eval = _eval;
  self->super.super.free_fn = _free;

  if (!_extract_args(self, args, error))
    goto error;

  filterx_function_args_free(args);
  return &self->super;

error:
  filterx_function_args_free(args);
  filterx_expr_unref(&self->super.super);
  return NULL;
}
