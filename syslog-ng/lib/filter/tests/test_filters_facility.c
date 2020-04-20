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

typedef struct _FilterParamBits
{
  const gchar *msg;
  const gchar *fac_str;
  gboolean    expected_result;
} FilterParamBits;

ParameterizedTestParameters(filter, test_filter_facility_str)
{
  static FilterParamBits test_data_list[] =
  {
    {.msg = "<15> openvpn[2499]: PTHREAD support initialized", .fac_str = "user", .expected_result = TRUE},
    {.msg = "<15> openvpn[2499]: PTHREAD support initialized", .fac_str = "daemon", .expected_result = FALSE},
    {.msg = "<2> openvpn[2499]: PTHREAD support initialized",  .fac_str = "kern", .expected_result = TRUE},
    {.msg = "<128> openvpn[2499]: PTHREAD support initialized", .fac_str = "local0", .expected_result = TRUE},
    {.msg = "<32> openvpn[2499]: PTHREAD support initialized", .fac_str = "local1", .expected_result = FALSE},
    {.msg = "<32> openvpn[2499]: PTHREAD support initialized", .fac_str = "auth", .expected_result = TRUE},
#ifdef LOG_AUTHPRIV
    {.msg = "<80> openvpn[2499]: PTHREAD support initialized", .fac_str = "authpriv", .expected_result = TRUE},
#endif
  };

  return cr_make_param_array(FilterParamBits, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(FilterParamBits *param, filter, test_filter_facility_str)
{
  FilterExprNode *filter = filter_facility_new(facility_bits(param->fac_str));
  testcase(param->msg, filter, param->expected_result);
}

typedef struct _FilterParamFacilities
{
  const gchar *msg;
  guint32     facilities;
  gboolean    expected_result;
} FilterParamFacilities;

ParameterizedTestParameters(filter, test_filter_facility)
{
  static FilterParamFacilities test_data_list[] =
  {
    {.msg = "<15> openvpn[2499]: PTHREAD support initialized", .facilities = 0x80000000 | (LOG_USER >> 3), .expected_result = TRUE},
    {.msg = "<15> openvpn[2499]: PTHREAD support initialized", .facilities = 0x80000000 | (LOG_DAEMON >> 3), .expected_result = FALSE},
    {.msg = "<2> openvpn[2499]: PTHREAD support initialized",  .facilities = 0x80000000 | (LOG_KERN >> 3), .expected_result = TRUE},
    {.msg = "<128> openvpn[2499]: PTHREAD support initialized", .facilities = 0x80000000 | (LOG_LOCAL0 >> 3), .expected_result = TRUE},
    {.msg = "<32> openvpn[2499]: PTHREAD support initialized", .facilities = 0x80000000 | (LOG_AUTH >> 3), .expected_result = TRUE},
#ifdef LOG_AUTHPRIV
    {.msg = "<80> openvpn[2499]: PTHREAD support initialized", .facilities = 0x80000000 | (LOG_AUTHPRIV >> 3), .expected_result = TRUE},
#endif
  };

  return cr_make_param_array(FilterParamFacilities, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(FilterParamFacilities *param, filter, test_filter_facility)
{
  FilterExprNode *filter = filter_facility_new(param->facilities);
  testcase(param->msg, filter, param->expected_result);
}

Test(filter, test_filter_facility_bits)
{
  testcase("<15> openvpn[2499]: PTHREAD support initialized",
           filter_facility_new(facility_bits("daemon") | facility_bits("user")), TRUE);
  testcase("<15> openvpn[2499]: PTHREAD support initialized",
           filter_facility_new(facility_bits("uucp") | facility_bits("local4")), FALSE);
}
