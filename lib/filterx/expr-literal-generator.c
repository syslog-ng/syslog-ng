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

#include "filterx/expr-literal-generator.h"
#include "filterx/object-primitive.h"

struct FilterXLiteralGeneratorElem_
{
  FilterXExpr *key;
  FilterXExpr *value;
  gboolean cloneable;
};

FilterXLiteralGeneratorElem *
filterx_literal_generator_elem_new(FilterXExpr *key, FilterXExpr *value, gboolean cloneable)
{
  FilterXLiteralGeneratorElem *self = g_new0(FilterXLiteralGeneratorElem, 1);

  self->key = key;
  self->value = value;
  self->cloneable = cloneable;

  return self;
}

static void
_literal_generator_elem_free(FilterXLiteralGeneratorElem *self)
{
  filterx_expr_unref(self->key);
  filterx_expr_unref(self->value);
  g_free(self);
}


struct FilterXExprLiteralGenerator_
{
  FilterXExprGenerator super;
  GList *elements;
};

/* Takes reference of elements */
void
filterx_literal_generator_set_elements(FilterXExpr *s, GList *elements)
{
  FilterXExprLiteralGenerator *self = (FilterXExprLiteralGenerator *) s;

  g_assert(!self->elements);
  self->elements = elements;
}

static gboolean
_eval_elements(FilterXObject *fillable, GList *elements)
{
  for (GList *link = elements; link; link = link->next)
    {
      FilterXLiteralGeneratorElem *elem = (FilterXLiteralGeneratorElem *) link->data;

      FilterXObject *key = NULL;
      if (elem->key)
        {
          key = filterx_expr_eval_typed(elem->key);
          if (!key)
            return FALSE;
        }

      FilterXObject *value = filterx_expr_eval_typed(elem->value);
      if (!value)
        {
          filterx_object_unref(key);
          return FALSE;
        }

      if (elem->cloneable)
        {
          FilterXObject *cloned_value = filterx_object_clone(value);
          filterx_object_unref(value);
          value = cloned_value;
        }

      gboolean success = filterx_object_set_subscript(fillable, key, &value);

      filterx_object_unref(key);
      filterx_object_unref(value);

      if (!success)
        return FALSE;
    }

  return TRUE;
}

static FilterXObject *
_dict_generator_create_container(FilterXExprGenerator *s, FilterXExpr *fillable_parent)
{
  FilterXObject *fillable_parent_obj = filterx_expr_eval_typed(fillable_parent);
  if (!fillable_parent_obj)
    return NULL;

  FilterXObject *result = filterx_object_create_dict(fillable_parent_obj);
  filterx_object_unref(fillable_parent_obj);
  return result;
}

static FilterXObject *
_list_generator_create_container(FilterXExprGenerator *s, FilterXExpr *fillable_parent)
{
  FilterXObject *fillable_parent_obj = filterx_expr_eval_typed(fillable_parent);
  if (!fillable_parent_obj)
    return NULL;

  FilterXObject *result = filterx_object_create_list(fillable_parent_obj);
  filterx_object_unref(fillable_parent_obj);
  return result;
}

static gboolean
_literal_generator_generate(FilterXExprGenerator *s, FilterXObject *fillable)
{
  FilterXExprLiteralGenerator *self = (FilterXExprLiteralGenerator *) s;

  return _eval_elements(fillable, self->elements);
}

void
_literal_generator_free(FilterXExpr *s)
{
  FilterXExprLiteralGenerator *self = (FilterXExprLiteralGenerator *) s;

  g_list_free_full(self->elements, (GDestroyNotify) _literal_generator_elem_free);
  filterx_generator_free_method(s);
}

static void
_literal_generator_init_instance(FilterXExprLiteralGenerator *self)
{
  filterx_generator_init_instance(&self->super.super);
  self->super.generate = _literal_generator_generate;
  self->super.super.free_fn = _literal_generator_free;
}

FilterXExpr *
filterx_literal_dict_generator_new(void)
{
  FilterXExprLiteralGenerator *self = g_new0(FilterXExprLiteralGenerator, 1);

  _literal_generator_init_instance(self);
  self->super.create_container = _dict_generator_create_container;

  return &self->super.super;
}

FilterXExpr *
filterx_literal_list_generator_new(void)
{
  FilterXExprLiteralGenerator *self = g_new0(FilterXExprLiteralGenerator, 1);

  _literal_generator_init_instance(self);
  self->super.create_container = _list_generator_create_container;

  return &self->super.super;
}


typedef struct FilterXLiteralInnerGenerator_
{
  FilterXExpr super;
  FilterXExprLiteralGenerator *root_literal_generator;
  GList *elements;
} FilterXLiteralInnerGenerator;

void
_literal_inner_generator_free(FilterXExpr *s)
{
  FilterXLiteralInnerGenerator *self = (FilterXLiteralInnerGenerator *) s;

  g_list_free_full(self->elements, (GDestroyNotify) _literal_generator_elem_free);
  filterx_expr_free_method(s);
}

static void
_literal_inner_generator_init_instance(FilterXLiteralInnerGenerator *self, FilterXExpr *root_literal_generator,
                                       GList *elements)
{
  filterx_expr_init_instance(&self->super);
  self->super.free_fn = _literal_inner_generator_free;

  /*
   * We do not ref or unref the root_literal_generator, as we are always accessed through that, so it is expected
   * to be alive while we are alive.
   */
  self->root_literal_generator = (FilterXExprLiteralGenerator *) root_literal_generator;
  self->elements = elements;
}

static FilterXObject *
_inner_dict_generator_eval(FilterXExpr *s)
{
  FilterXLiteralInnerGenerator *self = (FilterXLiteralInnerGenerator *) s;

  FilterXObject *root_fillable = filterx_expr_eval_typed(self->root_literal_generator->super.fillable);
  if (!root_fillable)
    return NULL;

  FilterXObject *fillable = filterx_object_create_dict(root_fillable);
  filterx_object_unref(root_fillable);
  if (!fillable)
    return NULL;

  if (_eval_elements(fillable, self->elements))
    return fillable;

  filterx_object_unref(fillable);
  return NULL;
}

static FilterXObject *
_inner_list_generator_eval(FilterXExpr *s)
{
  FilterXLiteralInnerGenerator *self = (FilterXLiteralInnerGenerator *) s;

  FilterXObject *root_fillable = filterx_expr_eval_typed(self->root_literal_generator->super.fillable);
  if (!root_fillable)
    return NULL;

  FilterXObject *fillable = filterx_object_create_list(root_fillable);
  filterx_object_unref(root_fillable);
  if (!fillable)
    return NULL;

  if (_eval_elements(fillable, self->elements))
    return fillable;

  filterx_object_unref(fillable);
  return NULL;
}

/* Takes reference of elements */
FilterXExpr *
filterx_literal_inner_dict_generator_new(FilterXExpr *root_literal_generator, GList *elements)
{
  FilterXLiteralInnerGenerator *self = g_new0(FilterXLiteralInnerGenerator, 1);

  _literal_inner_generator_init_instance(self, root_literal_generator, elements);
  self->super.eval = _inner_dict_generator_eval;

  return &self->super;
}

/* Takes reference of elements */
FilterXExpr *
filterx_literal_inner_list_generator_new(FilterXExpr *root_literal_generator, GList *elements)
{
  FilterXLiteralInnerGenerator *self = g_new0(FilterXLiteralInnerGenerator, 1);

  _literal_inner_generator_init_instance(self, root_literal_generator, elements);
  self->super.eval = _inner_list_generator_eval;

  return &self->super;
}
