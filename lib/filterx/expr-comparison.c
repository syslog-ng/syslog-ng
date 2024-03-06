/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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

#include "filterx/expr-comparison.h"
#include "filterx/object-datetime.h"
#include "filterx/filterx-globals.h"
#include "filterx/object-primitive.h"
#include "filterx/object-null.h"
#include "filterx/object-string.h"
#include "filterx/object-json.h"
#include "filterx/object-datetime.h"
#include "filterx/object-message-value.h"
#include "object-primitive.h"
#include "generic-number.h"
#include "parse-number.h"
#include "scratch-buffers.h"

typedef struct _FilterXComparison
{
  FilterXBinaryOp super;
  gint operator;
} FilterXComparison;

static void
convert_filterx_object_to_generic_number(FilterXObject *obj, GenericNumber *gn)
{
  if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(integer)) ||
      filterx_object_is_type(obj, &FILTERX_TYPE_NAME(double)) ||
      filterx_object_is_type(obj, &FILTERX_TYPE_NAME(boolean)))
    *gn = filterx_primitive_get_value(obj);
  else if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(string)))
    {
      if (!parse_generic_number(filterx_string_get_value(obj, NULL), gn))
        gn_set_nan(gn);
    }
  else if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(null)))
    gn_set_int64(gn, 0);
  else if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(bytes)) ||
           filterx_object_is_type(obj, &FILTERX_TYPE_NAME(protobuf)) ||
           filterx_object_is_type(obj, &FILTERX_TYPE_NAME(message_value)) ||
           filterx_object_is_type(obj, &FILTERX_TYPE_NAME(json)))
    gn_set_nan(gn);
  else if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(datetime)))
    {
      const UnixTime utime = filterx_datetime_get_value(obj);
      uint64_t unix_epoch = unix_time_to_unix_epoch(utime);
      gn_set_int64(gn, (uint64_t)unix_epoch);
    }
  else
    {
      gn_set_nan(gn);
    }
}

static void
convert_filterx_object_to_string(FilterXObject *obj, GString *dump)
{
  if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(string)))
    {
      gsize len;
      const gchar *str = filterx_string_get_value(obj, &len);
      g_string_assign(dump, str);
    }
  else if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(bytes)))
    {
      gsize len;
      const gchar *str = filterx_bytes_get_value(obj, &len);
      g_string_assign(dump, str);
    }
  else if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(protobuf)))
    {
      gsize len;
      const gchar *str = filterx_protobuf_get_value(obj, &len);
      g_string_assign(dump, str);
    }
  else
    {
      LogMessageValueType lmvt;
      if (!filterx_object_marshal(obj, dump, &lmvt))
        return;
    }
}

static gboolean
evaluate_comparison(gint cmp, gint operator)
{
  gboolean result = FALSE;
  if (cmp == 0)
    {
      result = operator & FCMPX_EQ;
    }
  else if (cmp < 0)
    {
      result = !!(operator & FCMPX_LT);
    }
  else
    {
      result = !!(operator & FCMPX_GT);
    }
  return result;

}

static gboolean
evaluate_comparison_gn(generic_number_cmp cmp, gint operator)
{
  if (cmp.is_nan)
    return (operator & FCMPX_NE) == FCMPX_NE;
  return evaluate_comparison(cmp.cmp, operator);
}

static gint
cmp_as_string(FilterXObject *lhs, FilterXObject *rhs)
{
  gint result = 0;
  GString *lhs_repr = scratch_buffers_alloc();
  GString *rhs_repr = scratch_buffers_alloc();
  convert_filterx_object_to_string(lhs, lhs_repr);
  convert_filterx_object_to_string(rhs, rhs_repr);
  if ((result = (lhs_repr->len - rhs_repr->len), result == 0))
    result = memcmp(lhs_repr->str, rhs_repr->str, MIN(lhs_repr->len, rhs_repr->len));
  return result;
}

static generic_number_cmp
cmp_as_num(FilterXObject *lhs, FilterXObject *rhs)
{
  GenericNumber lhs_number, rhs_number;
  convert_filterx_object_to_generic_number(lhs, &lhs_number);
  convert_filterx_object_to_generic_number(rhs, &rhs_number);

  if (gn_is_nan(&lhs_number) || gn_is_nan(&rhs_number))
    {
      return (generic_number_cmp)
      {
        .is_nan = TRUE
      };
    }
  else
    return (generic_number_cmp)
    {
      .cmp = gn_compare(&lhs_number, &rhs_number)
    };
}

static gboolean
evaluate_typed(FilterXObject *lhs, FilterXObject *rhs, gint operator)
{
  if (filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(string)) ||
      filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(bytes)) ||
      filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(protobuf)) ||
      filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(message_value)) ||
      filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(json)))
    return evaluate_comparison(cmp_as_string(lhs, rhs), operator);

  if (filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(null)) ||
      filterx_object_is_type(rhs, &FILTERX_TYPE_NAME(null))
     )
    {
      if (operator == FCMPX_NE)
        return lhs->type != rhs->type;
      if (operator == FCMPX_EQ)
        return lhs->type == rhs->type;
    }

  return evaluate_comparison_gn(cmp_as_num(lhs, rhs), operator);
}

static gboolean
evaluate_type_and_value(FilterXObject *lhs, FilterXObject *rhs, gint operator)
{
  if (operator == FCMPX_EQ)
    {
      if (lhs->type != rhs->type)
        return FALSE;
    }
  else if (operator == FCMPX_NE)
    {
      if (lhs->type != rhs->type)
        return TRUE;
    }
  else
    g_assert_not_reached();
  return evaluate_typed(lhs, rhs, operator);

}

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXComparison *self = (FilterXComparison *) s;

  FilterXObject *lhs_object = filterx_expr_eval(self->super.lhs);
  if (!lhs_object)
    return NULL;

  FilterXObject *rhs_object = filterx_expr_eval(self->super.rhs);
  if (!rhs_object)
    {
      filterx_object_unref(lhs_object);
      return NULL;
    }

  gint compare_mode = self->operator & FCMPX_MODE_MASK;
  gint operator = self->operator & FCMPX_OP_MASK;

  gboolean result = TRUE;
  if (compare_mode & FCMPX_TYPE_AWARE)
    result = evaluate_typed(lhs_object, rhs_object, operator);
  else if (compare_mode & FCMPX_STRING_BASED)
    result = evaluate_comparison(cmp_as_string(lhs_object, rhs_object), operator);
  else if (compare_mode & FCMPX_NUM_BASED)
    result = evaluate_comparison_gn(cmp_as_num(lhs_object, rhs_object), operator);
  else if (compare_mode & FCMPX_TYPE_AND_VALUE_BASED)
    result = evaluate_type_and_value(lhs_object, rhs_object, operator);
  else
    g_assert_not_reached();

  filterx_object_unref(lhs_object);
  filterx_object_unref(rhs_object);
  return filterx_boolean_new(result);
}

/* NOTE: takes the object reference */
FilterXExpr *
filterx_comparison_new(FilterXExpr *lhs, FilterXExpr *rhs, gint operator)
{
  FilterXComparison *self = g_new0(FilterXComparison, 1);

  filterx_binary_op_init_instance(&self->super, lhs, rhs);
  self->super.super.eval = _eval;
  self->operator = operator;
  return &self->super.super;
}
