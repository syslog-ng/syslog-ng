/*
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Kokan
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
#include <criterion/criterion.h>

#include "filter/filter-expr-parser.h"
#include "cfg-lexer.h"
#include "filter/optimizer/filter-expr-optimizer.h"
#include "filter/optimizer/concatenate-or-filters.h"
#include "apphook.h"


gint counter;

static gpointer
_dummy_init(FilterExprNode *root)
{
  ++counter;

  return (gpointer)&counter;
}

static void
_dummy_deinit(gpointer cookie)
{
  ++counter;
}

static void
_dummy_cb(FilterExprNode *current, FilterExprNode *parent, GPtrArray *childs, gpointer cookie)
{
  ++counter;
}

FilterExprOptimizer dummy =
{
  .name = "dummy",
  .init =  _dummy_init,
  .deinit = _dummy_deinit,
  .cb = _dummy_cb
};

static FilterExprNode *_compile_standalone_filter(gchar *config_snippet)
{
  GlobalConfig *cfg = cfg_new_snippet();
  CfgLexer *lexer = cfg_lexer_new_buffer(cfg, config_snippet, strlen(config_snippet));
  FilterExprNode *tmp;
  cr_assert(cfg_run_parser(cfg, lexer, &filter_expr_parser, (gpointer *) &tmp, NULL));

  cfg_free(cfg);
  return tmp;
}


Test(filter_optimizer, simple_filter)
{
  app_startup();
  FilterExprNode *expr = _compile_standalone_filter("program('foo');");

  cr_assert(filter_expr_optimizer_run(expr,  &dummy));
  cr_assert_eq(counter, 3, "%d==%d", counter, 3);

  filter_expr_unref(expr);
  app_shutdown();
}

Test(filter_optimizer, multiple_filter_expr)
{
  app_startup();
  FilterExprNode *expr = _compile_standalone_filter("program('foo') and message('blaze');");

  cr_assert(filter_expr_optimizer_run(expr,  &dummy));
  cr_assert_eq(counter, 5);

  filter_expr_unref(expr);
  app_shutdown();
}


TestSuite(filter_optimizer, .init = app_startup, .fini = app_shutdown);

Test(filter_optimizer, no_optimize)
{
  app_startup();
  FilterExprNode *expr = _compile_standalone_filter("program('foo');");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  &concatenate_or_filters);

  cr_assert_eq(expr, result);

  filter_expr_unref(expr);
  app_shutdown();
}

Test(filter_optimizer, same_filter_expr_with_and)
{
  app_startup();
  FilterExprNode *expr = _compile_standalone_filter("program('foo') and program('boo');");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  &concatenate_or_filters);

  cr_assert_eq(expr, result);

  filter_expr_unref(expr);
  app_shutdown();
}

Test(filter_optimizer, same_filter_expr_with_or)
{
  app_startup();
  FilterExprNode *expr = _compile_standalone_filter("program('foo') or program('boo');");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  &concatenate_or_filters);

  cr_assert_neq(expr, result);

  filter_expr_unref(result);
  app_shutdown();
}



