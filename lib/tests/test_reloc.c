/*
 * Copyright (c) 2002-2018 Balabit
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
#include <criterion/parameterized.h>
#include "reloc.h"

CacheResolver *path_resolver;
Cache *path_cache;

typedef struct _LookupParameter
{
  const gchar *template;
  const gchar *expected;
} LookupParameter;

ParameterizedTestParameters(reloc, test_path)
{
  static LookupParameter test_data_list[] =
  {
    {"/opt/syslog-ng", "/opt/syslog-ng"}, /* absoulte path remains unchanged */
    {"${prefix}/bin", "/test/bin"}, /* variables are resolved */
    {"/foo/${prefix}/bar", "/foo//test/bar"}, /* variables are resolved in the middle */
    {"${foo}/bin", "/foo/bin"}, /* variables are resolved recursively */
    {"${bar}/bin", "/foo/bin"} /* variables are resolved recursively */
  };

  return cr_make_param_array(LookupParameter, test_data_list, sizeof(test_data_list) / sizeof(test_data_list[0]));
}

ParameterizedTest(LookupParameter *test_data, reloc, test_path)
{
  const gchar *result;
  result = cache_lookup(path_cache, test_data->template);
  cr_assert_str_eq(result, test_data->expected, "Expanded install path (%s) doesn't match expected value (%s)", result,
                   test_data->expected);
}

static void
setup(void)
{
  path_resolver = path_resolver_new("/test");
  path_cache = cache_new(path_resolver);

  path_resolver_add_configure_variable(path_resolver, "${foo}", "/foo");
  path_resolver_add_configure_variable(path_resolver, "${bar}", "${foo}");
}

static void
teardown(void)
{
  cache_free(path_cache);
}

TestSuite(reloc, .init = setup, .fini = teardown);
