/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#include "filter/filter-cmp.h"
#include "filter/filter-expr-grammar.h"
#include "scratch-buffers.h"
#include "generic-number.h"
#include "parse-number.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct _FilterCmp
{
  FilterExprNode super;
  LogTemplate *left, *right;
  gint compare_mode;
} FilterCmp;

gint
fop_compare_numeric(const gchar *left, const gchar *right)
{
  gint l = atoi(left);
  gint r = atoi(right);
  if (l == r)
    return 0;
  else if (l < r)
    return -1;

  return 1;
}

gint
fop_compare_string(const gchar *left, const gchar *right)
{
  return strcmp(left, right);
}

static inline gboolean
_is_integer(LogMessageValueType type)
{
  return (type == LM_VT_INT32) || (type == LM_VT_INT64);
}

static inline gboolean
_is_floating_point(LogMessageValueType type)
{
  return type == LM_VT_DOUBLE;
}

static inline gboolean
_is_number(LogMessageValueType type)
{
  return _is_integer(type) || _is_floating_point(type);
}

static inline gboolean
_is_string(LogMessageValueType type)
{
  return type == LM_VT_STRING;
}

static gboolean
_evaluate_comparison(FilterCmp *self, gint cmp)
{
  gboolean result = FALSE;

  if (cmp == 0)
    {
      result = self->compare_mode & FCMP_EQ;
    }
  else if (cmp < 0)
    {
      result = !!(self->compare_mode & FCMP_LT);
    }
  else
    {
      result = !!(self->compare_mode & FCMP_GT);
    }
  return result;
}


/* NOTE: this function mimics JavaScript when converting a value to a
 * number.  As described here:
 *
 *   Primitive types: https://javascript.info/type-conversions
 *   Objects: https://javascript.info/object-toprimitive
 */
static void
_convert_to_number(const gchar *value, LogMessageValueType type, GenericNumber *number)
{
  switch (type)
    {
    case LM_VT_STRING:
    case LM_VT_INT32:
    case LM_VT_INT64:
    case LM_VT_DOUBLE:
      if (!parse_generic_number(value, number))
        gn_set_nan(number);
      break;
    case LM_VT_JSON:
    case LM_VT_LIST:
      /* JSON objects are always considered true, hence 1 */
      gn_set_int64(number, 1);
      break;
    case LM_VT_NULL:
      /* nulls objects are always considered 0 */
      gn_set_int64(number, 0);
      break;
    case LM_VT_BOOLEAN:
    {
      gboolean b;

      if (!type_cast_to_boolean(value, &b, NULL))
        gn_set_int64(number, b);
      else
        gn_set_int64(number, 0);
      break;
    }
    case LM_VT_DATETIME:
    {
      gint64 msec;

      if (type_cast_to_datetime_msec(value, &msec, NULL))
        gn_set_int64(number, msec);
      else
        gn_set_int64(number, 0);
      break;
    }
    default:
      g_assert_not_reached();
    }
}

static gboolean
_evaluate_typed(FilterCmp *self, const gchar *left, LogMessageValueType left_type, const gchar *right, LogMessageValueType right_type)
{
  GenericNumber l, r;

  /* Type aware comparison:
   *   - strings are compared as strings.
   *   - non-numbers are compared as strings.
   *   - numbers or mismatching types are compared as numbers */

  if (left_type == right_type &&
      (_is_string(left_type) || !_is_number(left_type)))
    return _evaluate_comparison(self, fop_compare_string(left, right));

  /* ok, we need to convert to numbers as compare that way */

  _convert_to_number(left, left_type, &l);
  _convert_to_number(right, right_type, &r);

  if (gn_is_nan(&l) && gn_is_nan(&r))
    {
      /* NaN is equal to itself but nothing else */
      return _evaluate_comparison(self, 0);
    }
  else if (gn_is_nan(&l) || gn_is_nan(&r))
    {
      /* if we have NaN on either side, we return FALSE in any comparisons */
      return FALSE;
    }

  return _evaluate_comparison(self, gn_compare(&l, &r));
}

static gboolean
fop_cmp_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg, LogTemplateEvalOptions *options)
{
  FilterCmp *self = (FilterCmp *) s;
  LogMessageValueType left_type, right_type;

  ScratchBuffersMarker marker;
  GString *left_buf = scratch_buffers_alloc_and_mark(&marker);
  GString *right_buf = scratch_buffers_alloc();

  log_template_append_format_value_and_type_with_context(self->left, msgs, num_msg, options, left_buf, &left_type);
  log_template_append_format_value_and_type_with_context(self->right, msgs, num_msg, options, right_buf, &right_type);

  gboolean result;
  if (self->compare_mode & FCMP_NUM)
    {
      result = _evaluate_typed(self, left_buf->str, left_type, right_buf->str, right_type);
    }
  else
    {
      result = _evaluate_comparison(self, fop_compare_string(left_buf->str, right_buf->str));
    }

  msg_trace("cmp() evaluation result",
            evt_tag_str("left", left_buf->str),
            evt_tag_str("operator", self->super.type),
            evt_tag_str("right", right_buf->str),
            evt_tag_int("result", result),
            evt_tag_printf("msg", "%p", msgs[num_msg - 1]));

  scratch_buffers_reclaim_marked(marker);
  return result ^ s->comp;
}

static void
fop_cmp_free(FilterExprNode *s)
{
  FilterCmp *self = (FilterCmp *) s;

  log_template_unref(self->left);
  log_template_unref(self->right);
  g_free((gchar *) self->super.type);
}

FilterExprNode *
fop_cmp_clone(FilterExprNode *s)
{
  FilterCmp *self = (FilterCmp *) s;

  FilterCmp *cloned_self = g_new0(FilterCmp, 1);
  filter_expr_node_init_instance(&cloned_self->super);

  cloned_self->super.eval = fop_cmp_eval;
  cloned_self->super.free_fn = fop_cmp_free;
  cloned_self->super.clone = fop_cmp_clone;
  cloned_self->left = log_template_ref(self->left);
  cloned_self->right = log_template_ref(self->right);
  cloned_self->compare_mode = self->compare_mode;
  cloned_self->super.type = g_strdup(self->super.type);

  return &cloned_self->super;
}

FilterExprNode *
fop_cmp_new(LogTemplate *left, LogTemplate *right, const gchar *type, gint compare_mode)
{
  FilterCmp *self = g_new0(FilterCmp, 1);

  filter_expr_node_init_instance(&self->super);
  self->super.type = g_strdup(type);
  self->compare_mode = compare_mode;

  if (self->compare_mode & FCMP_NUM && cfg_is_config_version_older(left->cfg, VERSION_VALUE_3_8))
    {
      msg_warning("WARNING: due to a bug in versions before " VERSION_3_8
                  "numeric comparison operators like '!=' in filter "
                  "expressions were evaluated as string operators. This is fixed in " VERSION_3_8 ". "
                  "As we are operating in compatibility mode, syslog-ng will exhibit the buggy "
                  "behaviour as previous versions until you bump the @version value in your "
                  "configuration file");
      self->compare_mode &= ~FCMP_NUM;
    }

  self->super.eval = fop_cmp_eval;
  self->super.free_fn = fop_cmp_free;
  self->super.clone = fop_cmp_clone;
  self->left = left;
  self->right = right;

  return &self->super;
}
