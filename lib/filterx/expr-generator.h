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

#ifndef FILTERX_EXPR_GENERATOR_H_INCLUDED
#define FILTERX_EXPR_GENERATOR_H_INCLUDED

#include "filterx/filterx-expr.h"

typedef struct FilterXExprGenerator_ FilterXExprGenerator;
struct FilterXExprGenerator_
{
  FilterXExpr super;
  FilterXExpr *fillable;
  gboolean (*generate)(FilterXExprGenerator *self, FilterXObject *fillable);
  FilterXObject *(*create_container)(FilterXExprGenerator *self, FilterXExpr *fillable_parent);
};

void filterx_generator_set_fillable(FilterXExpr *s, FilterXExpr *fillable);
void filterx_generator_init_instance(FilterXExpr *s);
void filterx_generator_free_method(FilterXExpr *s);

FilterXExpr *filterx_generator_create_container_new(FilterXExpr *g, FilterXExpr *fillable_parent);

#endif
