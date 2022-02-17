/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2013 Gergely Nagy <algernon@balabit.hu>
 * Copyright (c) 2021 Balazs Scheidler <bazsi77@gmail.com>
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
#include "libtest/cr_template.h"

#include "logmsg/logmsg.h"
#include "apphook.h"
#include "filter/filter-cmp.h"
#include "filter/filter-expr-grammar.h"

LogMessage *
_construct_filter_msg(void)
{
  LogMessage *msg = log_msg_new_empty();

  msg->pri = 15;
  log_msg_set_value(msg, LM_V_PROGRAM, "software", -1);
  return msg;
}

gboolean
evaluate_testcase(FilterExprNode *filter_node)
{
  LogMessage *msg = _construct_filter_msg();
  gboolean result;

  cr_assert_not_null(filter_node, "Constructing an in-list filter");
  result = filter_expr_eval(filter_node, msg);

  log_msg_unref(msg);
  filter_expr_unref(filter_node);
  return result;
}

gboolean
evaluate(const gchar *left, gint operator, const gchar *right)
{
  return evaluate_testcase(fop_cmp_new(compile_template(left), compile_template(right), operator));
}

Test(filter, test_num_eq)
{
  cr_assert(evaluate("10", KW_NUM_EQ, "10"));
  cr_assert(evaluate("$SEVERITY_NUM", KW_NUM_EQ, "7"));
  cr_assert(evaluate("$SEVERITY_NUM", KW_NUM_EQ, "$SEVERITY_NUM"));

  cr_assert_not(evaluate("10", KW_NUM_EQ, "11"));

}
Test(filter, test_num_ne)
{
  cr_assert(evaluate("10", KW_NUM_NE, "9"));
  cr_assert(evaluate("$SEVERITY_NUM", KW_NUM_NE, "8"));
  cr_assert(evaluate("$SEVERITY_NUM", KW_NUM_NE, "$FACILITY_NUM"));

  cr_assert_not(evaluate("10", KW_NUM_NE, "10"));
}

Test(filter, test_num_lt)
{
  cr_assert(evaluate("10", KW_NUM_LT, "11"));
  cr_assert(evaluate("7", KW_NUM_LT, "8"));
  cr_assert(evaluate("7", KW_NUM_LT, "10"));
  cr_assert(evaluate("$LEVEL_NUM", KW_NUM_LT, "8"));
  cr_assert(evaluate("$LEVEL_NUM", KW_NUM_LT, "10"));

  cr_assert_not(evaluate("11", KW_NUM_LT, "10"));
  cr_assert_not(evaluate("11", KW_NUM_LT, "11"));
}

Test(filter, test_num_le)
{
  cr_assert(evaluate("11", KW_NUM_LE, "11"));
  cr_assert(evaluate("10", KW_NUM_LE, "11"));
  cr_assert(evaluate("7", KW_NUM_LE, "8"));
  cr_assert(evaluate("7", KW_NUM_LE, "10"));
  cr_assert(evaluate("$LEVEL_NUM", KW_NUM_LE, "8"));
  cr_assert(evaluate("$LEVEL_NUM", KW_NUM_LE, "10"));

  cr_assert_not(evaluate("11", KW_NUM_LE, "10"));
}

Test(filter, test_num_gt)
{
  cr_assert(evaluate("11", KW_NUM_GT, "10"));
  cr_assert(evaluate("8", KW_NUM_GT, "7"));
  cr_assert(evaluate("10", KW_NUM_GT, "7"));
  cr_assert(evaluate("8", KW_NUM_GT, "$LEVEL_NUM"));
  cr_assert(evaluate("10", KW_NUM_GT, "$LEVEL_NUM"));

  cr_assert_not(evaluate("10", KW_NUM_GT, "11"));
  cr_assert_not(evaluate("10", KW_NUM_GT, "10"));
}

Test(filter, test_num_ge)
{
  cr_assert(evaluate("10", KW_NUM_GE, "10"));
  cr_assert(evaluate("11", KW_NUM_GE, "10"));
  cr_assert(evaluate("8", KW_NUM_GE, "7"));
  cr_assert(evaluate("10", KW_NUM_GE, "7"));
  cr_assert(evaluate("8", KW_NUM_GE, "$LEVEL_NUM"));
  cr_assert(evaluate("10", KW_NUM_GE, "$LEVEL_NUM"));

  cr_assert_not(evaluate("10", KW_NUM_GE, "11"));
}

Test(filter, test_numeric_ordering)
{
  cr_assert_not(evaluate("10", KW_NUM_LT, "10"));
  cr_assert(evaluate("10", KW_NUM_LE, "10"));
  cr_assert(evaluate("10", KW_NUM_EQ, "10"));
  cr_assert(evaluate("10", KW_NUM_GE, "10"));
  cr_assert_not(evaluate("10", KW_NUM_GT, "10"));

  cr_assert(evaluate("10", KW_NUM_LT, "11"));
  cr_assert(evaluate("10", KW_NUM_LE, "11"));
  cr_assert_not(evaluate("10", KW_NUM_EQ, "11"));
  cr_assert_not(evaluate("10", KW_NUM_GE, "11"));
  cr_assert_not(evaluate("10", KW_NUM_GT, "11"));

  cr_assert_not(evaluate("11", KW_NUM_LT, "10"));
  cr_assert_not(evaluate("11", KW_NUM_LE, "10"));
  cr_assert_not(evaluate("11", KW_NUM_EQ, "10"));
  cr_assert(evaluate("11", KW_NUM_GE, "10"));
  cr_assert(evaluate("11", KW_NUM_GT, "10"));
}

/* string comparison */

Test(filter, test_eq)
{
  cr_assert(evaluate("10", KW_EQ, "10"));
  cr_assert(evaluate("$SEVERITY_NUM", KW_EQ, "7"));
  cr_assert(evaluate("$SEVERITY_NUM", KW_EQ, "$SEVERITY_NUM"));

  cr_assert_not(evaluate("10", KW_EQ, "11"));

}

Test(filter, test_ne)
{
  cr_assert(evaluate("10", KW_NE, "9"));
  cr_assert(evaluate("$SEVERITY_NUM", KW_NE, "8"));
  cr_assert(evaluate("$SEVERITY_NUM", KW_NE, "$FACILITY_NUM"));

  cr_assert_not(evaluate("10", KW_NE, "10"));
}

Test(filter, test_lt)
{
  cr_assert(evaluate("10", KW_LT, "11"));
  cr_assert(evaluate("7", KW_LT, "8"));
  cr_assert(evaluate("$LEVEL_NUM", KW_LT, "8"));

  cr_assert_not(evaluate("7", KW_LT, "10"));
  cr_assert_not(evaluate("11", KW_LT, "10"));
  cr_assert_not(evaluate("11", KW_LT, "11"));
}

Test(filter, test_le)
{
  cr_assert(evaluate("11", KW_LE, "11"));
  cr_assert(evaluate("10", KW_LE, "11"));
  cr_assert(evaluate("7", KW_LE, "8"));
  cr_assert(evaluate("$LEVEL_NUM", KW_LE, "8"));

  cr_assert_not(evaluate("7", KW_LE, "10"));
  cr_assert_not(evaluate("11", KW_LE, "10"));
  cr_assert_not(evaluate("$LEVEL_NUM", KW_LE, "10"));
}

Test(filter, test_gt)
{
  cr_assert(evaluate("11", KW_GT, "10"));
  cr_assert(evaluate("8", KW_GT, "7"));
  cr_assert(evaluate("8", KW_GT, "$LEVEL_NUM"));

  cr_assert_not(evaluate("10", KW_GT, "7"));
  cr_assert_not(evaluate("10", KW_GT, "11"));
  cr_assert_not(evaluate("10", KW_GT, "10"));
  cr_assert_not(evaluate("10", KW_GT, "$LEVEL_NUM"));
}

Test(filter, test_ge)
{
  cr_assert(evaluate("10", KW_GE, "10"));
  cr_assert(evaluate("11", KW_GE, "10"));
  cr_assert(evaluate("8", KW_GE, "7"));
  cr_assert(evaluate("8", KW_GE, "$LEVEL_NUM"));

  cr_assert_not(evaluate("10", KW_GE, "7"));
  cr_assert_not(evaluate("10", KW_GE, "11"));
  cr_assert_not(evaluate("10", KW_GE, "$LEVEL_NUM"));
}

Test(filter, test_string_ordering)
{
  cr_assert_not(evaluate("10", KW_LT, "10"));
  cr_assert(evaluate("10", KW_LE, "10"));
  cr_assert(evaluate("10", KW_EQ, "10"));
  cr_assert(evaluate("10", KW_GE, "10"));
  cr_assert_not(evaluate("10", KW_GT, "10"));

  cr_assert(evaluate("10", KW_LT, "11"));
  cr_assert(evaluate("10", KW_LE, "11"));
  cr_assert_not(evaluate("10", KW_EQ, "11"));
  cr_assert_not(evaluate("10", KW_GE, "11"));
  cr_assert_not(evaluate("10", KW_GT, "11"));

  cr_assert_not(evaluate("11", KW_LT, "10"));
  cr_assert_not(evaluate("11", KW_LE, "10"));
  cr_assert_not(evaluate("11", KW_EQ, "10"));
  cr_assert(evaluate("11", KW_GE, "10"));
  cr_assert(evaluate("11", KW_GT, "10"));
}

Test(filter, test_string_ordering_with_non_numbers)
{
  cr_assert(evaluate("alma", KW_LT, "korte"));
  cr_assert(evaluate("alma", KW_LE, "korte"));
  cr_assert_not(evaluate("alma", KW_EQ, "korte"));
  cr_assert_not(evaluate("alma", KW_GE, "korte"));
  cr_assert_not(evaluate("alma", KW_GT, "korte"));

  cr_assert_not(evaluate("korte", KW_LT, "alma"));
  cr_assert_not(evaluate("korte", KW_LE, "alma"));
  cr_assert_not(evaluate("korte", KW_EQ, "alma"));
  cr_assert(evaluate("korte", KW_GE, "alma"));
  cr_assert(evaluate("korte", KW_GT, "alma"));
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

TestSuite(filter, .init = setup, .fini = teardown);
