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

#ifndef FILTERX_EXPR_LITERAL_GENERATOR_H_INCLUDED
#define FILTERX_EXPR_LITERAL_GENERATOR_H_INCLUDED

#include "filterx/expr-generator.h"

typedef struct FilterXLiteralGeneratorElem_ FilterXLiteralGeneratorElem;
typedef struct FilterXExprLiteralGenerator_ FilterXExprLiteralGenerator;

FilterXLiteralGeneratorElem *filterx_literal_generator_elem_new(FilterXExpr *key, FilterXExpr *value,
    gboolean cloneable);

FilterXExpr *filterx_literal_dict_generator_new(void);
FilterXExpr *filterx_literal_list_generator_new(void);
void filterx_literal_generator_set_elements(FilterXExpr *s, GList *elements);

FilterXExpr *filterx_literal_inner_dict_generator_new(FilterXExpr *root_literal_generator, GList *elements);
FilterXExpr *filterx_literal_inner_list_generator_new(FilterXExpr *root_literal_generator, GList *elements);

#endif
