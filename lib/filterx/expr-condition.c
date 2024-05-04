/*
 * Copyright (c) 2023 shifter <shifter@axoflow.com>
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

#include "filterx/expr-condition.h"
#include "filterx/object-primitive.h"

static FilterXConditional *
_tail_condition(FilterXConditional *c)
{
  g_assert(c != NULL);
  if (c->false_branch != NULL)
    return _tail_condition(c->false_branch);
  else
    return c;
}

static FilterXObject *
_eval_condition(FilterXConditional *c)
{
  FilterXObject *result = NULL;
  FilterXObject *condition_value = NULL;
  if (c->condition != FILTERX_CONDITIONAL_NO_CONDITION)
    {
      condition_value = filterx_expr_eval(c->condition);
      if (!condition_value)
        return NULL;
      if (filterx_object_falsy(condition_value))
        {
          // no condition-expression match, no elif or else case
          // returning true to avoid filterx failure on non matching if's
          if (c->false_branch == NULL)
            result = filterx_boolean_new(TRUE);
          else
            result = _eval_condition(c->false_branch);
          goto exit;
        }
    }

  if (!c->statements)
    {
      if (c->condition)
        return condition_value;
      // returning true to avoid filterx failure on empty else block
      result = filterx_boolean_new(TRUE);
      goto exit;
    }

  if (!filterx_expr_list_eval(c->statements, &result))
    {
      filterx_object_unref(result);
      return filterx_boolean_new(FALSE);
    }
exit:
  filterx_object_unref(condition_value);
  return result;
}

static void
_free (FilterXExpr *s)
{
  FilterXConditional *self = (FilterXConditional *) s;

  filterx_expr_unref(self->condition);
  g_list_free_full(self->statements, (GDestroyNotify) filterx_expr_unref);
  if (self->false_branch != NULL)
    filterx_expr_unref(&self->false_branch->super);
  filterx_expr_free_method(s);
}

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXConditional *self = (FilterXConditional *) s;
  return _eval_condition(self);
}

FilterXExpr *
filterx_conditional_new_conditional_codeblock(FilterXExpr *condition, GList *stmts)
{
  FilterXConditional *self = g_new0(FilterXConditional, 1);
  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.free_fn = _free;
  self->condition = condition;
  self->statements = stmts;
  return &self->super;
}

FilterXExpr *
filterx_conditional_new_codeblock(GList *stmts)
{
  return filterx_conditional_new_conditional_codeblock(FILTERX_CONDITIONAL_NO_CONDITION, stmts);
}


FilterXExpr *
filterx_conditional_add_false_branch(FilterXConditional *s, FilterXConditional *false_branch)
{
  g_assert(s != NULL);
  FilterXConditional *last_condition = _tail_condition(s);

  // avoid to create nested elses (if () {} else {} else {} else {})
  g_assert(last_condition->condition);

  last_condition->false_branch = false_branch;
  return &s->super;
}
