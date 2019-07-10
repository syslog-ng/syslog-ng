/*
 * Copyright (c) 2019 Balabit
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

#ifndef FILTER_EXPR_OPTIMIZER_H_INCLUDED
#define FILTER_EXPR_OPTIMIZER_H_INCLUDED

#include "filter/filter-expr.h"
#include "filter/filter-call.h"

typedef struct _FilterExprOptimizer FilterExprOptimizer;

struct _FilterExprOptimizer
{
  const gchar *name;
  gpointer (*init)(FilterExprNode *root);
  void (*deinit)(gpointer cookie);
  FilterExprNodeTraversalCallbackFunction cb;
};

static inline FilterExprNode *
filter_expr_optimizer_run(FilterExprNode *self, FilterExprOptimizer *optimizer)
{
  // This create a dummy_root, which points to the original root, so in case of root needs to be replaced it can use dummy_root as parent.
  FilterExprNode *dummy_root = filter_call_direct_new(self);

  msg_debug("Initializing filter-optimizer", evt_tag_str("name", optimizer->name));
  gpointer cookie = optimizer->init(dummy_root);
  if (cookie == NULL)
    {
      msg_debug("Skipping filter-optimizer, because init failed", evt_tag_str("name", optimizer->name));
      return self;
    }

  msg_debug("Running filter-optimizer", evt_tag_str("name", optimizer->name));
  filter_expr_traversal(dummy_root, NULL, optimizer->cb, cookie);
  optimizer->deinit(cookie);
  FilterExprNode *result = filter_call_next(dummy_root);
  filter_expr_unref(dummy_root);
  return result;
}

#endif
