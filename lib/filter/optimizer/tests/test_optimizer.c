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


static FilterExprNode *
filter_dummy_new(void)
{
  FilterExprNode *self = g_new0(FilterExprNode, 1);

  filter_expr_node_init_instance(self);
  self->type = g_strdup("dummy");

  return self;
}

static gpointer
_replace_init(FilterExprNode *root)
{
  return (gpointer)root;
}

static void
_replace_deinit(gpointer cookie)
{

}

static void
_replace_cb(FilterExprNode *current, FilterExprNode *parent, GPtrArray *childs, gpointer cookie)
{
  FilterExprNode *node = filter_dummy_new();
  filter_expr_replace_child(parent, current, node);
}

FilterExprOptimizer always_replace_with_dummy_filter =
{
  .name = "replacer",
  .init =  _replace_init,
  .deinit = _replace_deinit,
  .cb = _replace_cb
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
  FilterExprNode *expr = _compile_standalone_filter("program('foo');");

  cr_assert(filter_expr_optimizer_run(expr,  &dummy));
  cr_assert_eq(counter, 3, "%d==%d", counter, 3);

  filter_expr_unref(expr);
}

Test(filter_optimizer, simple_negated_filter)
{
  FilterExprNode *expr = _compile_standalone_filter("not program('foo');");

  cr_assert(filter_expr_optimizer_run(expr,  &dummy));
  cr_assert_eq(counter, 3, "%d==%d", counter, 3);

  filter_expr_unref(expr);
}

Test(filter_optimizer, multiple_filter_expr)
{
  FilterExprNode *expr = _compile_standalone_filter("program('foo') and message('blaze');");

  cr_assert(filter_expr_optimizer_run(expr,  &dummy));
  cr_assert_eq(counter, 5);

  filter_expr_unref(expr);
}

Test(filter_optimizer, complex_filte)
{
  FilterExprNode *expr = _compile_standalone_filter("level(info) or (program('foo') and not message('blaze'));");

  cr_assert(filter_expr_optimizer_run(expr,  &dummy));
  cr_assert_eq(counter, 7);

  filter_expr_unref(expr);
}



TestSuite(filter_optimizer, .init = app_startup, .fini = app_shutdown);

Test(replace_optimizer, simple_filter)
{
  FilterExprNode *expr = _compile_standalone_filter("program('foo');");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  &always_replace_with_dummy_filter);

  cr_assert(result);
  cr_assert_str_eq(result->type, "dummy");

  filter_expr_unref(result);
}


Test(replace_optimizer, simple_negated_filter)
{
  FilterExprNode *expr = _compile_standalone_filter("not program('foo');");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  &always_replace_with_dummy_filter);

  cr_assert(result);
  cr_assert_str_eq(result->type, "dummy");

  filter_expr_unref(result);
}

Test(replace_optimizer, complex_filter)
{
  FilterExprNode *expr = _compile_standalone_filter("level(info) or (program('foo') and not message('blaze'));");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  &always_replace_with_dummy_filter);

  cr_assert(result);
  cr_assert_str_eq(result->type, "dummy");

  filter_expr_unref(result);
}

TestSuite(replace_optimizer, .init = app_startup, .fini = app_shutdown);


Test(filter_optimizer, no_optimize)
{
  FilterExprNode *expr = _compile_standalone_filter("program('foo');");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  concatenate_or_filters_get_instance());

  cr_assert_eq(expr, result);

  filter_expr_unref(expr);
}

Test(filter_optimizer, same_filter_expr_with_and)
{
  FilterExprNode *expr = _compile_standalone_filter("program('foo') and program('boo');");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  concatenate_or_filters_get_instance());

  cr_assert_eq(expr, result);

  filter_expr_unref(expr);
}

Test(filter_optimizer, same_filter_expr_with_or_but_one_negated)
{
  FilterExprNode *expr = _compile_standalone_filter("program('foo') or not program('boo');");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  concatenate_or_filters_get_instance());

  cr_assert_eq(expr, result);

  filter_expr_unref(result);
}

Test(filter_optimizer, different_filter_with_or)
{
  FilterExprNode *expr = _compile_standalone_filter("program('foo') or message('boo');");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  concatenate_or_filters_get_instance());

  cr_assert_eq(expr, result);

  filter_expr_unref(result);
}

Test(filter_optimizer, type_string_or_programs)
{
  FilterExprNode *expr = _compile_standalone_filter("program('f1' type(string)) or program('f2' type(string));");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  concatenate_or_filters_get_instance());

  cr_assert_str_eq(result->type, "pcre");
  cr_assert_str_eq(result->template, "$PROGRAM");
  cr_assert_str_eq(result->pattern, "f2|f1");
  cr_assert_eq(result->modify, FALSE);
  cr_assert_eq(result->comp, FALSE);

  filter_expr_unref(result);
}

Test(filter_optimizer, type_pcre_or_programs)
{
  FilterExprNode *expr = _compile_standalone_filter("program('f1' type(pcre)) or program('f2' type(pcre));");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  concatenate_or_filters_get_instance());

  cr_assert_str_eq(result->type, "pcre");
  cr_assert_str_eq(result->template, "$PROGRAM");
  cr_assert_str_eq(result->pattern, "f2|f1");
  cr_assert_eq(result->modify, FALSE);
  cr_assert_eq(result->comp, FALSE);

  filter_expr_unref(result);
}

Test(filter_optimizer, same_filter_expr_with_or)
{
  FilterExprNode *expr = _compile_standalone_filter("program('foo') or program('boo');");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  concatenate_or_filters_get_instance());

  cr_assert_str_eq(result->type, "pcre");
  cr_assert_str_eq(result->template, "$PROGRAM");
  cr_assert_str_eq(result->pattern, "boo|foo");
  cr_assert_eq(result->modify, FALSE);
  cr_assert_eq(result->comp, FALSE);

  filter_expr_unref(result);
}

Test(filter_optimizer, same_negated_filter_expr_with_or)
{
  FilterExprNode *expr = _compile_standalone_filter("not program('foo') or not program('boo');");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  concatenate_or_filters_get_instance());

  cr_assert_str_eq(result->type, "pcre");
  cr_assert_str_eq(result->template, "$PROGRAM");
  cr_assert_str_eq(result->pattern, "boo|foo");
  cr_assert_eq(result->modify, FALSE);
  cr_assert_eq(result->comp, TRUE);

  filter_expr_unref(result);
}

Test(filter_optimizer, multiple_or_filter)
{
  FilterExprNode *expr = _compile_standalone_filter("program('f1') or program('f2') or program('f3');");

  FilterExprNode *result = filter_expr_optimizer_run(expr,  concatenate_or_filters_get_instance());

  cr_assert_str_eq(result->type, "pcre");
  cr_assert_str_eq(result->template, "$PROGRAM");
  cr_assert_str_eq(result->pattern, "f3|f2|f1");
  cr_assert_eq(result->modify, FALSE);
  cr_assert_eq(result->comp, FALSE);

  filter_expr_unref(result);
}



