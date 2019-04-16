/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2013 Gergely Nagy <algernon@balabit.hu>
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
#include "filter/filter-expr.h"
#include "filter/filter-re.h"
#include "filter/filter-pri.h"
#include "filter/filter-op.h"
#include "filter/filter-cmp.h"
#include "filter/filter-expr-grammar.h"
#include "cfg.h"
#include "test_filters_common.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

TestSuite(filter, .init = setup, .fini = teardown);

typedef struct _FilterParamFopCmp
{
  const gchar *msg;
  const gchar *template1;
  const gchar *template2;
  gint op;
  gboolean expected_result;
} FilterParamFopCmp;


ParameterizedTestParameters(filter, test_filter_fop_cmp)
{
  static FilterParamFopCmp test_data_list[] =
  {
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "$LEVEL_NUM", .template2 = "7", .op = KW_NUM_EQ, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "$LEVEL_NUM", .template2 = "5", .op = KW_NUM_NE, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "$LEVEL_NUM", .template2 = "8", .op = KW_NUM_LT, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "$LEVEL_NUM", .template2 = "10", .op = KW_NUM_LT, .expected_result = TRUE},
    /* 7 lt 10 is FALSE as 10 orders lower when interpreted as a string */
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "$LEVEL_NUM", .template2 = "10", .op = KW_LT, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "$LEVEL_NUM", .template2 = "5", .op = KW_NUM_GT, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "$LEVEL_NUM", .template2 = "7", .op = KW_NUM_GE, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "$LEVEL_NUM", .template2 = "7", .op = KW_NUM_LE, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "alma", .template2 = "korte", .op = KW_LT, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "alma", .template2 = "korte", .op = KW_LE, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "alma", .template2 = "korte", .op = KW_EQ, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "alma", .template2 = "korte", .op = KW_NE, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "alma", .template2 = "korte", .op = KW_GE, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "alma", .template2 = "korte", .op = KW_GT, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "korte", .template2 = "alma", .op = KW_LT, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "korte", .template2 = "alma", .op = KW_LE, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "korte", .template2 = "alma", .op = KW_EQ, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "korte", .template2 = "alma", .op = KW_NE, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "korte", .template2 = "alma", .op = KW_GE, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "korte", .template2 = "alma", .op = KW_GT, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "alma", .template2 = "alma", .op = KW_LT, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "alma", .template2 = "alma", .op = KW_LE, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "alma", .template2 = "alma", .op = KW_EQ, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "alma", .template2 = "alma", .op = KW_NE, .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "alma", .template2 = "alma", .op = KW_GE, .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:17:01 host openvpn[2499]: PTHREAD support initialized", .template1 = "alma", .template2 = "alma", .op = KW_GT, .expected_result = FALSE},
  };

  return cr_make_param_array(FilterParamFopCmp, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(FilterParamFopCmp *param, filter, test_filter_fop_cmp)
{
  FilterExprNode *filter = fop_cmp_new(create_template(param->template1), create_template(param->template2), param->op);
  testcase(param->msg, filter, param->expected_result);
}
