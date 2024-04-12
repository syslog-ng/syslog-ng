/*
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

#include "filterx/expr-regexp.h"
#include "filterx/object-primitive.h"

typedef struct FilterXExprRegexpMatch_
{
  FilterXExpr super;
  FilterXExpr *lhs;
  gchar *pattern;
} FilterXExprRegexpMatch;

static FilterXObject *
_regexp_match_eval(FilterXExpr *s)
{
  FilterXExprRegexpMatch *self = (FilterXExprRegexpMatch *) s;

  /* TODO: regexp match */
  return filterx_boolean_new(TRUE);
}

static void
_regexp_match_free(FilterXExpr *s)
{
  FilterXExprRegexpMatch *self = (FilterXExprRegexpMatch *) s;

  filterx_expr_unref(self->lhs);
  g_free(self->pattern);
  filterx_expr_free_method(s);
}

/* Takes reference of lhs */
FilterXExpr *
filterx_expr_regexp_match_new(FilterXExpr *lhs, const gchar *pattern)
{
  FilterXExprRegexpMatch *self = g_new0(FilterXExprRegexpMatch, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _regexp_match_eval;
  self->super.free_fn = _regexp_match_free;

  self->lhs = lhs;
  self->pattern = g_strdup(pattern);

  return &self->super;
}


typedef struct FilterXExprRegexpSearchGenerator_
{
  FilterXExprGenerator super;
  FilterXExpr *lhs;
  gchar *pattern;
} FilterXExprRegexpSearchGenerator;

static FilterXObject *
_regexp_search_generator_eval(FilterXExpr *s)
{
  FilterXExprRegexpSearchGenerator *self = (FilterXExprRegexpSearchGenerator *) s;

  FilterXObject *fillable = filterx_expr_eval_typed(self->super.fillable);
  if (!fillable)
    return NULL;

  /* TODO: regexp match */
  FilterXObject *result = filterx_boolean_new(TRUE);
  filterx_object_unref(fillable);
  return result;
}

static FilterXObject *
_regexp_search_generator_create_container(FilterXExprGenerator *s, FilterXExpr *fillable_parent)
{
  FilterXExprRegexpSearchGenerator *self = (FilterXExprRegexpSearchGenerator *) s;

  FilterXObject *fillable_parent_obj = filterx_expr_eval_typed(fillable_parent);
  if (!fillable_parent_obj)
    return NULL;

  /* TODO: if (has_named_capture_groups) */
  FilterXObject *result = filterx_object_create_dict(fillable_parent_obj);
  filterx_object_unref(fillable_parent_obj);
  return result;
}

static void
_regexp_search_generator_free(FilterXExpr *s)
{
  FilterXExprRegexpSearchGenerator *self = (FilterXExprRegexpSearchGenerator *) s;

  filterx_expr_unref(self->lhs);
  g_free(self->pattern);
  filterx_generator_free_method(s);
}

/* Takes reference of lhs */
FilterXExpr *
filterx_expr_regexp_search_generator_new(FilterXExpr *lhs, const gchar *pattern)
{
  FilterXExprRegexpSearchGenerator *self = g_new0(FilterXExprRegexpSearchGenerator, 1);

  filterx_generator_init_instance(&self->super.super);
  self->super.super.eval = _regexp_search_generator_eval;
  self->super.super.free_fn = _regexp_search_generator_free;
  self->super.create_container = _regexp_search_generator_create_container;

  self->lhs = lhs;
  self->pattern = g_strdup(pattern);

  return &self->super.super;
}
