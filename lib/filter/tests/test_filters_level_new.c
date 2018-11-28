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
#include "cfg.h"
#include "test_filters_common.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

TestSuite(filter, .init = setup, .fini = teardown);

typedef struct _FilterParamRange
{
  const gchar *msg;
  const gchar *from;
  const gchar *to;
  gboolean    expected_result;
} FilterParamRange;

ParameterizedTestParameters(filter, test_filter_level_range)
{
  static FilterParamRange test_data_list[] =
  {
    {.msg = "<15> openvpn[2499]: PTHREAD support initialized", .from = "debug", .to = "emerg", .expected_result = TRUE},
    {.msg = "<8> openvpn[2499]: PTHREAD support initialized", .from = "crit", .to = "emerg", .expected_result = TRUE},
    {.msg = "<9> openvpn[2499]: PTHREAD support initialized", .from = "crit", .to = "emerg", .expected_result = TRUE},
    {.msg = "<10> openvpn[2499]: PTHREAD support initialized", .from = "crit", .to = "emerg", .expected_result = TRUE},
    {.msg = "<11> openvpn[2499]: PTHREAD support initialized", .from = "crit", .to = "emerg", .expected_result = FALSE},
    {.msg = "<12> openvpn[2499]: PTHREAD support initialized", .from = "crit", .to = "emerg", .expected_result = FALSE},
    {.msg = "<13> openvpn[2499]: PTHREAD support initialized", .from = "crit", .to = "emerg", .expected_result = FALSE},
    {.msg = "<14> openvpn[2499]: PTHREAD support initialized", .from = "crit", .to = "emerg", .expected_result = FALSE},
    {.msg = "<15> openvpn[2499]: PTHREAD support initialized", .from = "crit", .to = "emerg", .expected_result = FALSE},
    {.msg = "<8> openvpn[2499]: PTHREAD support initialized", .from = "debug", .to = "notice", .expected_result = FALSE},
    {.msg = "<9> openvpn[2499]: PTHREAD support initialized", .from = "debug", .to = "notice", .expected_result = FALSE},
    {.msg = "<10> openvpn[2499]: PTHREAD support initialized", .from = "debug", .to = "notice", .expected_result = FALSE},
    {.msg = "<11> openvpn[2499]: PTHREAD support initialized", .from = "debug", .to = "notice", .expected_result = FALSE},
    {.msg = "<12> openvpn[2499]: PTHREAD support initialized", .from = "debug", .to = "notice", .expected_result = FALSE},
    {.msg = "<13> openvpn[2499]: PTHREAD support initialized", .from = "debug", .to = "notice", .expected_result = TRUE},
    {.msg = "<14> openvpn[2499]: PTHREAD support initialized", .from = "debug", .to = "notice", .expected_result = TRUE},
    {.msg = "<15> openvpn[2499]: PTHREAD support initialized", .from = "debug", .to = "notice", .expected_result = TRUE}
  };

  return cr_make_param_array(FilterParamRange, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(FilterParamRange *param, filter, test_filter_level_range)
{
  FilterExprNode *filter = filter_level_new(level_range(param->from, param->to));
  testcase(param->msg, filter, param->expected_result);
}


typedef struct _FilterParamBits
{
  const gchar *msg;
  const gchar *lev;
  gboolean    expected_result;
} FilterParamBits;

ParameterizedTestParameters(filter, test_filter_level_bits)
{
  static FilterParamBits test_data_list[] =
  {
    {.msg = "<15> openvpn[2499]: PTHREAD support initialized", .lev = "emerg", .expected_result = FALSE},
    {.msg = "<0> openvpn[2499]: PTHREAD support initialized", .lev = "emerg", .expected_result = TRUE},
    {.msg = "<1> openvpn[2499]: PTHREAD support initialized", .lev = "alert", .expected_result = TRUE},
    {.msg = "<2> openvpn[2499]: PTHREAD support initialized", .lev = "crit", .expected_result = TRUE},
    {.msg = "<3> openvpn[2499]: PTHREAD support initialized", .lev = "err", .expected_result = TRUE},
    {.msg = "<4> openvpn[2499]: PTHREAD support initialized", .lev = "warning", .expected_result = TRUE},
    {.msg = "<5> openvpn[2499]: PTHREAD support initialized", .lev = "notice", .expected_result = TRUE},
    {.msg = "<6> openvpn[2499]: PTHREAD support initialized", .lev = "info", .expected_result = TRUE},
    {.msg = "<7> openvpn[2499]: PTHREAD support initialized", .lev = "debug", .expected_result = TRUE}
  };

  return cr_make_param_array(FilterParamBits, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(FilterParamBits *param, filter, test_filter_level_bits)
{
  FilterExprNode *filter = filter_level_new(level_bits(param->lev));
  testcase(param->msg, filter, param->expected_result);
}
