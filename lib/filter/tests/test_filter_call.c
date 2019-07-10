/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 Kokan
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
#include "filter/filter-call.h"
#include "filter/filter-pri.h"
#include "filter/filter-expr.h"
#include "apphook.h"

#include <criterion/criterion.h>


Test(filter_call, undefined_filter_ref)
{
  FilterExprNode *filter = filter_call_new("undefined_filter", configuration);
  cr_assert_not_null(filter);

  cr_assert_not(filter_expr_init(filter, configuration));

  filter_expr_unref(filter);
}

Test(filter_call, replace_existing_child)
{
  FilterExprNode *old = filter_level_new(0);
  FilterExprNode *new = filter_level_new(0);

  FilterExprNode *filter = filter_call_direct_new( old );

  filter_expr_replace_child(filter, old, new);

  FilterExprNode *filter_next = filter_call_next(filter);
  cr_assert_eq(filter_next, new, "Filter call didn't replace the child element.");

  filter_expr_unref(filter_next);
  filter_expr_unref(filter);
}

static void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
}

static void
teardown(void)
{
  app_shutdown();
  cfg_free(configuration);
}

TestSuite(filter_call, .init = setup, .fini = teardown);

