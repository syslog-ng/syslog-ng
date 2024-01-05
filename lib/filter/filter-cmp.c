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

static gint
fop_compare_numeric(const GString *left, const GString *right)
{
  gint l = atoi(left->str);
  gint r = atoi(right->str);
  if (l == r)
    return 0;
  else if (l < r)
    return -1;

  return 1;
}

static gint
fop_compare_bytes(const GString *left, const GString *right)
{
  gint cmp = memcmp(left->str, right->str, MIN(left->len, right->len));
  if (cmp != 0)
    return cmp;

  if (left->len == right->len)
    return 0;

  return left->len < right->len ? -1 : 1;
}

static inline gboolean
_is_object(LogMessageValueType type)
{
  return type == LM_VT_JSON || type == LM_VT_LIST;
}

static inline gboolean
_is_string(LogMessageValueType type)
{
  return type == LM_VT_STRING;
}

static inline gboolean
_is_bytes(LogMessageValueType type)
{
  return type == LM_VT_BYTES || type == LM_VT_PROTOBUF;
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
 * Primitive types: https://javascript.info/type-conversions
 *
 * Objects are always converted to NaNs as we can't evaluate the
 * toPrimitive() method, even if the Object had one.  Here's is how it is
 * dealt with in JavaScript: https://javascript.info/object-toprimitive
 *
 */
static void
_convert_to_number(const GString *value, LogMessageValueType type, GenericNumber *number)
{
  switch (type)
    {
    case LM_VT_STRING:
    case LM_VT_INTEGER:
    case LM_VT_DOUBLE:
      if (!parse_generic_number(value->str, number))
        gn_set_nan(number);
      break;
    case LM_VT_JSON:
    case LM_VT_LIST:
    case LM_VT_BYTES:
    case LM_VT_PROTOBUF:
      gn_set_nan(number);
      break;
    case LM_VT_NULL:
      gn_set_int64(number, 0);
      break;
    case LM_VT_BOOLEAN:
    {
      gboolean b;

      if (type_cast_to_boolean(value->str, -1, &b, NULL))
        gn_set_int64(number, b);
      else
        gn_set_int64(number, 0);
      break;
    }
    case LM_VT_DATETIME:
    {
      gint64 msec;

      if (type_cast_to_datetime_msec(value->str, -1, &msec, NULL))
        gn_set_int64(number, msec);
      else
        gn_set_int64(number, 0);
      break;
    }
    default:
      g_assert_not_reached();
    }
}

/*
 * The "new" 4.0 comparison operators have become type aware, e.g.  when
 * doing comparisons they consult with the types associated with the arguments.
 *
 * The algorithm took an inspiration from JavaScript, but it deviates
 * somewhat, here is how it works.
 *
 * 1) if both arguments are the same type, then strings and objects/lists are
 *    compared as their string representation (in JavaScript objects would
 *    not be equal as they each are separate instances and JS compares
 *    references in this case)
 *
 * 2) if both arguments are of the NULL type, they would evaluate to TRUE if
 *    both sides are NULL (e.g.  we can compare NULLs to NULLs)
 *
 * 3) otherwise the arguments are converted to numbers (as per JavaScript
 *    behavior) and compared numerically.
 *
 *    3.a) If any or both sides becomes a NaN (ie: not-a-number) the
 *    evaluation is always FALSE to match JavaScript behavior
 *
 *    3.b) If one side is a NaN and we check for not-equal, then the
 *    evaluation is always TRUE, again to match JavaScript behaviour.
 *    (<anything> != NaN is always TRUE even if <anything> is NaN too)
 *
 */
static gboolean
_evaluate_typed(FilterCmp *self,
                const GString *left, LogMessageValueType left_type,
                const GString *right, LogMessageValueType right_type)
{
  GenericNumber l, r;

  /* Type aware comparison:
   *   - strings are compared as strings.
   *   - objects (ie. non-numbers) are compared as strings.
   *   - numbers or mismatching types are compared as numbers */

  if (left_type == right_type &&
      (_is_string(left_type) || _is_object(left_type) || _is_bytes(left_type)))
    return _evaluate_comparison(self, fop_compare_bytes(left, right));

  if (left_type == LM_VT_NULL || right_type == LM_VT_NULL)
    {
      /* != */
      if ((self->compare_mode & FCMP_OP_MASK) == (FCMP_LT + FCMP_GT))
        return left_type != right_type;
      /* == */
      if ((self->compare_mode & FCMP_OP_MASK) == FCMP_EQ)
        return left_type == right_type;

      /* fallback to numeric comparisons */
    }

  /* ok, we need to convert to numbers and compare that way */

  _convert_to_number(left, left_type, &l);
  _convert_to_number(right, right_type, &r);

  if (gn_is_nan(&l) || gn_is_nan(&r))
    {
      /* NaN == NaN is false */
      /* NaN > NaN is false */
      /* NaN < NaN is false */
      /* NaN != NaN is true */

      /* != is handled specially */
      if ((self->compare_mode & (FCMP_LT + FCMP_GT)) == (FCMP_LT + FCMP_GT))
        return TRUE;
      /* if we have NaN on either side, we return FALSE in any comparisons */
      return FALSE;
    }

  return _evaluate_comparison(self, gn_compare(&l, &r));
}

static gboolean
_evaluate_type_and_value_comparison(FilterCmp *self,
                                    const GString *left, LogMessageValueType left_type,
                                    const GString *right, LogMessageValueType right_type)
{
  if ((self->compare_mode & FCMP_OP_MASK) == FCMP_EQ)
    {
      /* === */
      if (left_type != right_type)
        return FALSE;
    }
  else if ((self->compare_mode & FCMP_OP_MASK) == (FCMP_LT + FCMP_GT))
    {
      /* !== */
      if (left_type != right_type)
        return TRUE;
    }
  else
    g_assert_not_reached();
  return _evaluate_typed(self, left, left_type, right, right_type);
}


static const gchar *
_compare_mode_to_string(gint compare_mode)
{
  if (compare_mode & FCMP_TYPE_AWARE)
    return "type-aware";
  else if (compare_mode & FCMP_STRING_BASED)
    return "string";
  else if (compare_mode & FCMP_NUM_BASED)
    return "numeric";
  g_assert_not_reached();
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
  if (self->compare_mode & FCMP_TYPE_AWARE)
    result = _evaluate_typed(self, left_buf, left_type, right_buf, right_type);
  else if (self->compare_mode & FCMP_STRING_BASED)
    result = _evaluate_comparison(self, fop_compare_bytes(left_buf, right_buf));
  else if (self->compare_mode & FCMP_NUM_BASED)
    result = _evaluate_comparison(self, fop_compare_numeric(left_buf, right_buf));
  else if (self->compare_mode & FCMP_TYPE_AND_VALUE_BASED)
    result = _evaluate_type_and_value_comparison(self, left_buf, left_type, right_buf, right_type);
  else
    g_assert_not_reached();

  msg_trace("cmp() evaluation result",
            evt_tag_str("left", left_buf->str),
            evt_tag_str("operator", self->super.type),
            evt_tag_str("right", right_buf->str),
            evt_tag_str("compare_mode", _compare_mode_to_string(self->compare_mode)),
            evt_tag_str("left_type", log_msg_value_type_to_str(left_type)),
            evt_tag_str("right_type", log_msg_value_type_to_str(right_type)),
            evt_tag_int("result", result),
            evt_tag_msg_reference(msgs[num_msg - 1]));

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
fop_cmp_new(LogTemplate *left, LogTemplate *right, const gchar *type, gint compare_mode, const gchar *location)
{
  FilterCmp *self = g_new0(FilterCmp, 1);

  filter_expr_node_init_instance(&self->super);
  self->super.type = g_strdup(type);
  self->super.eval = fop_cmp_eval;
  self->super.free_fn = fop_cmp_free;
  self->super.clone = fop_cmp_clone;
  self->compare_mode = compare_mode;
  self->left = left;
  self->right = right;

  if ((self->compare_mode & FCMP_TYPE_AWARE) &&
      cfg_is_config_version_older(left->cfg, VERSION_VALUE_4_0))
    {
      if (self->left->explicit_type_hint != LM_VT_NONE || self->right->explicit_type_hint != LM_VT_NONE)
        {
          /* the user has used explicit types in this expression, so let's
           * keep it type aware and suppress any warnings.  */
        }
      else
        {
          if (cfg_is_typing_feature_enabled(configuration))
            {
              msg_warning("WARNING: syslog-ng comparisons became type-aware starting with " VERSION_4_0 " "
                          "which means that syslog-ng attempts to infer the intended type of an "
                          "expression automatically and performs comparisons according to the types detected, "
                          "similarly how JavaScript evaluates the comparison of potentially mismatching types. "
                          "You seem to be using numeric operators in this filter expression, so "
                          "please make sure that once the type-aware behavior is turned on it remains correct, "
                          "see this blog post for more information: https://syslog-ng-future.blog/syslog-ng-4-theme-typing/",
                          evt_tag_str("location", location));
            }
          self->compare_mode = (self->compare_mode & ~FCMP_TYPE_AWARE) | FCMP_NUM_BASED;
        }
    }

  if ((self->compare_mode & FCMP_NUM_BASED) && cfg_is_config_version_older(left->cfg, VERSION_VALUE_3_8))
    {
      msg_warning("WARNING: due to a bug in versions before " VERSION_3_8 ", "
                  "numeric comparison operators like '!=' in filter "
                  "expressions were evaluated as string operators. This is fixed in " VERSION_3_8 ". "
                  "As we are operating in compatibility mode, syslog-ng will exhibit the buggy "
                  "behaviour as previous versions until you bump the @version value in your "
                  "configuration file",
                  evt_tag_str("location", location));
      self->compare_mode &= ~FCMP_TYPE_AWARE;
      self->compare_mode |= FCMP_STRING_BASED;
    }


  g_assert((self->compare_mode & FCMP_MODE_MASK) != 0);

  return &self->super;
}
