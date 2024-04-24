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
#include "filterx/filterx-globals.h"

typedef struct FilterXFunctionIsType_
{
  FilterXFunction super;
  FilterXExpr *lhs;
  FilterXType *type;
} FilterXFunctionIsType;

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXFunctionIsType *self = (FilterXFunctionIsType *) s;

  FilterXObject *lhs = filterx_expr_eval(self->lhs);
  if (!lhs)
    {
      msg_error("FilterX: istype: Failed to check type of object");
      return NULL;
    }

  gboolean result = filterx_object_is_type(lhs, self->type);
  filterx_object_unref(lhs);
  return filterx_boolean_new(result);
}

static void
_free(FilterXExpr *s)
{
  FilterXFunctionIsType *self = (FilterXFunctionIsType *) s;
  filterx_expr_unref(self->lhs);
  filterx_function_free_method(&self->super);
}

static FilterXType *
_extract_type(GList *argument_expressions)
{
  if (argument_expressions == NULL || g_list_length(argument_expressions) != 2)
    {
      msg_error("FilterX: istype: invalid number of arguments. "
                "Usage: istype(object, type_str)");
      return NULL;
    }

  FilterXExpr *type_expr = (FilterXExpr *) argument_expressions->next->data;
  if (!type_expr || !filterx_expr_is_literal(type_expr))
    {
      msg_error("FilterX: istype: invalid argument: type_str "
                "Usage: istype(object, type_str)");
      return NULL;
    }

  FilterXObject *type_obj = filterx_expr_eval(type_expr);
  if (!type_obj)
    {
      msg_error("FilterX: istype: invalid argument: type_str "
                "Usage: istype(object, type_str)");
      return NULL;
    }

  gsize type_len;
  const gchar *type = filterx_string_get_value(type_obj, &type_len);
  filterx_object_unref(type_obj);

  if (!type)
    {
      msg_error("FilterX: istype: invalid argument: type_str "
                "Usage: istype(object, type_str)");
      return NULL;
    }

  FilterXType *fxtype = filterx_type_lookup(type);
  if (!fxtype)
    {
      msg_error("FilterX: istype: unknown type name",
                evt_tag_str("type", type));
      return NULL;
    }

  return fxtype;
}

FilterXFunction *
filterx_function_istype_new(const gchar *function_name, GList *argument_expressions)
{
  FilterXFunctionIsType *self = g_new0(FilterXFunctionIsType, 1);
  filterx_function_init_instance(&self->super, function_name);
  self->super.super.eval = _eval;
  self->super.super.free_fn = _free;

  self->type = _extract_type(argument_expressions);
  if (!self->type)
    goto error;

  self->lhs = filterx_expr_ref(((FilterXExpr *) argument_expressions->data));
  if (!self->lhs)
    goto error;

  g_list_free_full(argument_expressions, (GDestroyNotify) filterx_expr_unref);
  return &self->super;

error:
  g_list_free_full(argument_expressions, (GDestroyNotify) filterx_expr_unref);
  filterx_expr_unref(&self->super.super);
  return NULL;
}
