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
_construct_sample_message(void)
{
  LogMessage *msg = log_msg_new_empty();

  msg->pri = 15;
  log_msg_set_value(msg, LM_V_PROGRAM, "software", -1);
  return msg;
}

FilterExprNode *
_construct_filter(const gchar *config_snippet)
{
  FilterExprNode *tmp;

  CfgLexer *lexer = cfg_lexer_new_buffer(configuration, config_snippet, strlen(config_snippet));
  cr_assert(lexer);

  cr_assert(cfg_run_parser(configuration, lexer, &filter_expr_parser, (gpointer *) &tmp, NULL));
  cr_assert(tmp);
  return tmp;
}

gboolean
evaluate(const gchar *filter_expr)
{
  LogMessage *msg = _construct_sample_message();
  FilterExprNode *filter_node = _construct_filter(filter_expr);
  gboolean result;

  cr_assert_not_null(filter_node, "Error compiling filter expression");
  result = filter_expr_eval(filter_node, msg);

  log_msg_unref(msg);
  filter_expr_unref(filter_node);
  return result;
}

Test(filter, test_num_eq)
{
  cr_assert(evaluate("10 == 10"));
  cr_assert(evaluate("$SEVERITY_NUM == 7"));
  cr_assert(evaluate("$SEVERITY_NUM == $SEVERITY_NUM"));

  cr_assert_not(evaluate("10 == 11"));
}

Test(filter, test_num_ne)
{
  cr_assert(evaluate("10 != 9"));
  cr_assert(evaluate("$SEVERITY_NUM != 8"));
  cr_assert(evaluate("$SEVERITY_NUM != $FACILITY_NUM"));

  cr_assert_not(evaluate("10 != 10"));
}

Test(filter, test_num_lt)
{
  cr_assert(evaluate("10 < 11"));
  cr_assert(evaluate("7 < 8"));
  cr_assert(evaluate("7 < 10"));
  cr_assert(evaluate("$LEVEL_NUM < 8"));
  cr_assert(evaluate("$LEVEL_NUM < 10"));

  cr_assert_not(evaluate("11 < 10"));
  cr_assert_not(evaluate("11 < 11"));
}

Test(filter, test_num_le)
{
  cr_assert(evaluate("11 <= 11"));
  cr_assert(evaluate("10 <= 11"));
  cr_assert(evaluate("7 <= 8"));
  cr_assert(evaluate("7 <= 10"));
  cr_assert(evaluate("$LEVEL_NUM <= 8"));
  cr_assert(evaluate("$LEVEL_NUM <= 10"));

  cr_assert_not(evaluate("11 <= 10"));
}

Test(filter, test_num_gt)
{
  cr_assert(evaluate("11 > 10"));
  cr_assert(evaluate("8 > 7"));
  cr_assert(evaluate("10 > 7"));
  cr_assert(evaluate("8 > $LEVEL_NUM"));
  cr_assert(evaluate("10 > $LEVEL_NUM"));

  cr_assert_not(evaluate("10 > 11"));
  cr_assert_not(evaluate("10 > 10"));
}

Test(filter, test_num_ge)
{
  cr_assert(evaluate("10 >= 10"));
  cr_assert(evaluate("11 >= 10"));
  cr_assert(evaluate("8 >= 7"));
  cr_assert(evaluate("10 >= 7"));
  cr_assert(evaluate("8 >= $LEVEL_NUM"));
  cr_assert(evaluate("10 >= $LEVEL_NUM"));

  cr_assert_not(evaluate("10 >= 11"));
}

Test(filter, test_numeric_ordering)
{
  cr_assert_not(evaluate("10 < 10"));
  cr_assert(evaluate("10 <= 10"));
  cr_assert(evaluate("10 == 10"));
  cr_assert(evaluate("10 >= 10"));
  cr_assert_not(evaluate("10 > 10"));

  cr_assert(evaluate("10 < 11"));
  cr_assert(evaluate("10 <= 11"));
  cr_assert_not(evaluate("10 == 11"));
  cr_assert_not(evaluate("10 >= 11"));
  cr_assert_not(evaluate("10 > 11"));

  cr_assert_not(evaluate("11 < 10"));
  cr_assert_not(evaluate("11 <= 10"));
  cr_assert_not(evaluate("11 == 10"));
  cr_assert(evaluate("11 >= 10"));
  cr_assert(evaluate("11 > 10"));
}

/* string comparison */

Test(filter, test_eq)
{
  cr_assert(evaluate("10 eq 10"));
  cr_assert(evaluate("$SEVERITY_NUM eq 7"));
  cr_assert(evaluate("$SEVERITY_NUM eq $SEVERITY_NUM"));

  cr_assert_not(evaluate("10 eq 11"));

}

Test(filter, test_ne)
{
  cr_assert(evaluate("10 ne 9"));
  cr_assert(evaluate("$SEVERITY_NUM ne 8"));
  cr_assert(evaluate("$SEVERITY_NUM ne $FACILITY_NUM"));

  cr_assert_not(evaluate("10 ne 10"));
}

Test(filter, test_lt)
{
  cr_assert(evaluate("10 lt 11"));
  cr_assert(evaluate("7 lt 8"));
  cr_assert(evaluate("$LEVEL_NUM lt 8"));

  cr_assert_not(evaluate("7 lt 10"));
  cr_assert_not(evaluate("11 lt 10"));
  cr_assert_not(evaluate("11 lt 11"));
}

Test(filter, test_le)
{
  cr_assert(evaluate("11 le 11"));
  cr_assert(evaluate("10 le 11"));
  cr_assert(evaluate("7 le 8"));
  cr_assert(evaluate("$LEVEL_NUM le 8"));

  cr_assert_not(evaluate("7 le 10"));
  cr_assert_not(evaluate("11 le 10"));
  cr_assert_not(evaluate("$LEVEL_NUM le 10"));
}

Test(filter, test_gt)
{
  cr_assert(evaluate("11 gt 10"));
  cr_assert(evaluate("8 gt 7"));
  cr_assert(evaluate("8 gt $LEVEL_NUM"));

  cr_assert_not(evaluate("10 gt 7"));
  cr_assert_not(evaluate("10 gt 11"));
  cr_assert_not(evaluate("10 gt 10"));
  cr_assert_not(evaluate("10 gt $LEVEL_NUM"));
}

Test(filter, test_ge)
{
  cr_assert(evaluate("10 ge 10"));
  cr_assert(evaluate("11 ge 10"));
  cr_assert(evaluate("8 ge 7"));
  cr_assert(evaluate("8 ge $LEVEL_NUM"));

  cr_assert_not(evaluate("10 ge 7"));
  cr_assert_not(evaluate("10 ge 11"));
  cr_assert_not(evaluate("10 ge $LEVEL_NUM"));
}

Test(filter, test_string_ordering)
{
  cr_assert_not(evaluate("10 lt 10"));
  cr_assert(evaluate("10 le 10"));
  cr_assert(evaluate("10 eq 10"));
  cr_assert(evaluate("10 ge 10"));
  cr_assert_not(evaluate("10 gt 10"));

  cr_assert(evaluate("10 lt 11"));
  cr_assert(evaluate("10 le 11"));
  cr_assert_not(evaluate("10 eq 11"));
  cr_assert_not(evaluate("10 ge 11"));
  cr_assert_not(evaluate("10 gt 11"));

  cr_assert_not(evaluate("11 lt 10"));
  cr_assert_not(evaluate("11 le 10"));
  cr_assert_not(evaluate("11 eq 10"));
  cr_assert(evaluate("11 ge 10"));
  cr_assert(evaluate("11 gt 10"));
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
