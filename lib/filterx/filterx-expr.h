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

#ifndef FILTERX_EXPR_H_INCLUDED
#define FILTERX_EXPR_H_INCLUDED

#include "filterx-object.h"
#include "template/eval.h"

typedef struct _FilterXExpr FilterXExpr;
typedef struct _FilterXEvalContext FilterXEvalContext;

struct _FilterXEvalContext
{
  LogMessage **msgs;
  gint num_msg;
  LogTemplateEvalOptions *template_eval_options;
};

struct _FilterXExpr
{
  guint32 ref_cnt;
  const gchar *type;
  FilterXObject *(*eval)(FilterXExpr *self);
  FilterXObject *(*eval_typed)(FilterXExpr *self);
  void (*free_fn)(FilterXExpr *self);
};

static inline FilterXObject *
filterx_expr_eval(FilterXExpr *self)
{
  return self->eval(self);
}

static inline FilterXObject *
filterx_expr_eval_typed(FilterXExpr *self)
{
  if (!self->eval_typed)
    return filterx_expr_eval(self);
  return self->eval_typed(self);
}

void filterx_expr_init_instance(FilterXExpr *self);
FilterXExpr *filterx_expr_new(void);
FilterXExpr *filterx_expr_ref(FilterXExpr *self);
void filterx_expr_unref(FilterXExpr *self);

#endif
