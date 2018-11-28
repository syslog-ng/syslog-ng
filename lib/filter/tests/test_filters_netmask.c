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
#include "filter/filter-netmask6.h"
#include "filter/filter-netmask.h"
#include "filter/filter-re.h"
#include "filter/filter-pri.h"
#include "filter/filter-op.h"
#include "filter/filter-cmp.h"
#include "cfg.h"
#include "test_filters_common.h"


#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

TestSuite(filter, .init = setup, .fini = teardown);

typedef struct _FilterParamNetmask
{
  const gchar *msg;
  const gchar *sockaddr;
  const gchar *cidr;
  gboolean    expected_result;
} FilterParamNetmask;

ParameterizedTestParameters(filter, test_filter_netmask_ip4_socket)
{
  static FilterParamNetmask test_data_list[] =
  {
    {.msg = "<15>Oct 15 16:19:01 host openvpn[2499]: PTHREAD support initialized", .sockaddr = "10.10.0.1", .cidr = "10.10.0.0/16", .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:19:02 host openvpn[2499]: PTHREAD support initialized", .sockaddr = "10.10.0.1", .cidr = "10.10.0.0/24", .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:19:03 host openvpn[2499]: PTHREAD support initialized", .sockaddr = "10.10.0.1", .cidr = "10.10.10.0/24", .expected_result = FALSE},
    {.msg = "<15>Oct 15 16:19:04 host openvpn[2499]: PTHREAD support initialized", .sockaddr = "10.10.0.1", .cidr = "0.0.10.10/24", .expected_result = FALSE},
  };

  return cr_make_param_array(FilterParamNetmask, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(FilterParamNetmask *param, filter, test_filter_netmask_ip4_socket)
{
  testcase_with_socket(param->msg, param->sockaddr, filter_netmask_new(param->cidr), param->expected_result);
}


ParameterizedTestParameters(filter, test_filter_netmask_ip4)
{
  static FilterParamNetmask test_data_list[] =
  {
    {.msg = "<15>Oct 15 16:20:01 host openvpn[2499]: PTHREAD support initialized", .cidr = "127.0.0.1/32", .expected_result = TRUE},
    {.msg = "<15>Oct 15 16:20:02 host openvpn[2499]: PTHREAD support initialized", .cidr = "127.0.0.2/32", .expected_result = FALSE},
  };

  return cr_make_param_array(FilterParamNetmask, test_data_list, G_N_ELEMENTS(test_data_list));
}

ParameterizedTest(FilterParamNetmask *param, filter, test_filter_netmask_ip4)
{
  testcase(param->msg, filter_netmask_new(param->cidr), param->expected_result);
}
