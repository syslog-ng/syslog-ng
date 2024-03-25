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
_convert_filterx_object_to_generic_number(FilterXObject *obj, GenericNumber *gn)
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

static const gchar *
_convert_filterx_object_to_string(FilterXObject *obj, gsize *len)
{
  if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(string)))
    {
      return filterx_string_get_value(obj, len);
    }
  else if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(bytes)))
    {
      return filterx_bytes_get_value(obj, len);
    }
  else if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(protobuf)))
    {
      return filterx_protobuf_get_value(obj, len);
    }

  GString *buffer = scratch_buffers_alloc();
  LogMessageValueType lmvt;
  if (!filterx_object_marshal(obj, buffer, &lmvt))
    return NULL;

  *len = buffer->len;
  return buffer->str;
}

static gboolean
_evaluate_comparison(gint cmp, gint operator)
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
_evaluate_as_string(FilterXObject *lhs, FilterXObject *rhs, gint operator)
{
  gsize lhs_len, rhs_len;
  const gchar *lhs_repr = _convert_filterx_object_to_string(lhs, &lhs_len);
  const gchar *rhs_repr = _convert_filterx_object_to_string(rhs, &rhs_len);

  gint result = memcmp(lhs_repr, rhs_repr, MIN(lhs_len, rhs_len));
  if (result == 0)
    result = lhs_len - rhs_len;
  return _evaluate_comparison(result, operator);
}

static gboolean
_evaluate_as_num(FilterXObject *lhs, FilterXObject *rhs, gint operator)
{
  GenericNumber lhs_number, rhs_number;
  _convert_filterx_object_to_generic_number(lhs, &lhs_number);
  _convert_filterx_object_to_generic_number(rhs, &rhs_number);

  if (gn_is_nan(&lhs_number) || gn_is_nan(&rhs_number))
    return operator == FCMPX_NE;
  return _evaluate_comparison(gn_compare(&lhs_number, &rhs_number), operator);
}

static gboolean
_evaluate_type_aware(FilterXObject *lhs, FilterXObject *rhs, gint operator)
{
  if (filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(string)) ||
      filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(bytes)) ||
      filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(protobuf)) ||
      filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(message_value)) ||
      filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(json_object)) || // TODO: we should have generic map and array cmp
      filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(json_array)))
    return _evaluate_as_string(lhs, rhs, operator);

  if (filterx_object_is_type(lhs, &FILTERX_TYPE_NAME(null)) ||
      filterx_object_is_type(rhs, &FILTERX_TYPE_NAME(null))
     )
    {
      if (operator == FCMPX_NE)
        return lhs->type != rhs->type;
      if (operator == FCMPX_EQ)
        return lhs->type == rhs->type;
    }

  return _evaluate_as_num(lhs, rhs, operator);
}

static gboolean
_evaluate_type_and_value_based(FilterXObject *lhs, FilterXObject *rhs, gint operator)
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
  return _evaluate_type_aware(lhs, rhs, operator);
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
    result = _evaluate_type_aware(lhs_object, rhs_object, operator);
  else if (compare_mode & FCMPX_STRING_BASED)
    result = _evaluate_as_string(lhs_object, rhs_object, operator);
  else if (compare_mode & FCMPX_NUM_BASED)
    result = _evaluate_as_num(lhs_object, rhs_object, operator);
  else if (compare_mode & FCMPX_TYPE_AND_VALUE_BASED)
    result = _evaluate_type_and_value_based(lhs_object, rhs_object, operator);
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
