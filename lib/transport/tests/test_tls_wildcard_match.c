/*
 * Copyright (c) 2024 One Identity LLC.
 * Copyright (c) 2024 Franco Fichtner
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

#include "transport/tls-verifier.h"

TestSuite(tls_wildcard, .init = NULL, .fini = NULL);

Test(tls_wildcard, test_wildcard_match_pattern_acceptance)
{
  cr_assert_eq(tls_wildcard_match("test", "test"), TRUE);
  cr_assert_eq(tls_wildcard_match("test", "*"), TRUE);
  cr_assert_eq(tls_wildcard_match("test", "t*t"), TRUE);
  cr_assert_eq(tls_wildcard_match("test", "t*"), TRUE);
  cr_assert_eq(tls_wildcard_match("", ""), TRUE);
  cr_assert_eq(tls_wildcard_match("test.one", "test.one"), TRUE);
  cr_assert_eq(tls_wildcard_match("test.one.two", "test.one.two"), TRUE);
}

Test(tls_wildcard, test_wildcard_match_wildcard_rejection)
{
  cr_assert_eq(tls_wildcard_match("test", "**"), FALSE);
  cr_assert_eq(tls_wildcard_match("test", "*es*"), FALSE);
  cr_assert_eq(tls_wildcard_match("test", "t*?"), FALSE);
}

Test(tls_wildcard, test_wildcard_match_pattern_rejection)
{
  cr_assert_eq(tls_wildcard_match("test", "tset"), FALSE);
  cr_assert_eq(tls_wildcard_match("test", "set"), FALSE);
  cr_assert_eq(tls_wildcard_match("", "*"), FALSE);
  cr_assert_eq(tls_wildcard_match("test", ""), FALSE);
  cr_assert_eq(tls_wildcard_match("test.two", "test.one"), FALSE);
}

Test(tls_wildcard, test_wildcard_match_format_rejection)
{
  cr_assert_eq(tls_wildcard_match("test.two", "test.*"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.two", "test.t*o"), FALSE);
  cr_assert_eq(tls_wildcard_match("test", "test.two"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.two", "test"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.one.two", "test.one"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.one", "test.one.two"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.three", "three.test"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.one.two", "test.one.*"), FALSE);
}

Test(tls_wildcard, test_wildcard_match_complex_rejection)
{
  cr_assert_eq(tls_wildcard_match("test.two", "test.???"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.one.two", "test.one.?wo"), FALSE);
}
