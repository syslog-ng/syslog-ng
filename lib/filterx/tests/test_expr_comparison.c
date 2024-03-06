/*
 * Copyright (c) 2023 shifter <shifter@axoflow.com>
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

#include "filterx/filterx-object.h"
#include "filterx/object-primitive.h"
#include "filterx/expr-comparison.h"
#include "filterx/filterx-expr.h"
#include "filterx/expr-literal.h"
#include "filterx/object-string.h"
#include "filterx/object-null.h"
#include "filterx/object-datetime.h"
#include "filterx/object-message-value.h"

#include "apphook.h"
#include "scratch-buffers.h"

static void _assert_comparison(FilterXObject *lhs, FilterXObject *rhs, gint operator, gboolean expected)
{
  FilterXExpr *lhse = filterx_literal_new(lhs);
  FilterXExpr *rhse = filterx_literal_new(rhs);
  FilterXExpr *cmp = filterx_comparison_new(lhse, rhse, operator);
  cr_assert_not_null(cmp);
  FilterXObject *result = filterx_expr_eval(cmp);
  cr_assert_not_null(result);
  cr_assert(filterx_object_truthy(result) == expected);
  filterx_expr_unref(cmp);
  // note that comparison expression unref operands
}

// FCMPX_NUM_BASED tests

Test(expr_comparison, test_integer_to_integer_num_based_comparison)
{
  _assert_comparison(filterx_integer_new(6), filterx_integer_new(6), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(0), filterx_integer_new(0), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(0), filterx_integer_new(6), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_integer_new(6), filterx_integer_new(6), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_integer_new(0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_integer_new(0), FCMPX_GT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(0), filterx_integer_new(5), FCMPX_GT | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_integer_new(6), filterx_integer_new(6), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_integer_new(0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_integer_new(0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_integer_new(7), FCMPX_LT | FCMPX_NUM_BASED, TRUE);

  _assert_comparison(filterx_integer_new(6), filterx_integer_new(6), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_integer_new(0), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_integer_new(6), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
}

Test(expr_comparison, test_integer_to_double_num_based_comparison)
{
  _assert_comparison(filterx_integer_new(6), filterx_double_new(6.0), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(0), filterx_double_new(0.0), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(6), filterx_double_new(3.14), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_integer_new(6), filterx_double_new(6.0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_double_new(0.0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_double_new(0.0), FCMPX_GT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(0), filterx_double_new(5.0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_integer_new(6), filterx_double_new(6.0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_double_new(0.0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_double_new(0.0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_double_new(7.0), FCMPX_LT | FCMPX_NUM_BASED, TRUE);

  _assert_comparison(filterx_integer_new(6), filterx_double_new(6.0), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_double_new(0.0), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_double_new(6.0), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
}

Test(expr_comparison, test_integer_to_string_num_based_comparison)
{
  _assert_comparison(filterx_integer_new(3), filterx_string_new("3", 1), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(0), filterx_string_new("0", 1), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("6", 1), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("7.1", 3), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("invalid", 5), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("empty", 0), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_integer_new(7), filterx_string_new("7", 1), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("7.0", 3), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_string_new("7", 1), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(8), filterx_string_new("7.1", 3), FCMPX_GT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("invalid", 5), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("empty", 0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_integer_new(7), filterx_string_new("7", 1), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("7.0", 3), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("0", 1), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("7.1", 3), FCMPX_LT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("invalid", 5), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("empty", 0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_integer_new(3), filterx_string_new("3", 1), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_string_new("0", 1), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("6", 1), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("7.1", 3), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("invalid", 5), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(7), filterx_string_new("empty", 0), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
}

Test(expr_comparison, test_integer_to_null_num_based_comparison)
{
  // NULL converted to 0 for some reason
  _assert_comparison(filterx_integer_new(7), filterx_null_new(), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_null_new(), FCMPX_GT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(7), filterx_null_new(), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_null_new(), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(0), filterx_null_new(), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(0), filterx_null_new(), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_null_new(), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_null_new(), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
}

Test(expr_comparison, test_integer_to_bool_num_based_comparison)
{
  _assert_comparison(filterx_integer_new(0), filterx_boolean_new(false), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(0), filterx_boolean_new(false), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_boolean_new(false), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_boolean_new(false), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(1), filterx_boolean_new(true), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(1), filterx_boolean_new(true), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(1), filterx_boolean_new(true), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(1), filterx_boolean_new(true), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
}

Test(expr_comparison, test_integer_to_datetime_num_based_comparison)
{
  UnixTime ut = { .ut_sec = 1701350398, .ut_usec = 123000, .ut_gmtoff = 3600 };
  guint64 epoch = unix_time_to_unix_epoch(ut);
  _assert_comparison(filterx_integer_new((uint64_t)(epoch)), filterx_datetime_new(&ut), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new((uint64_t)(epoch)), filterx_datetime_new(&ut), FCMPX_LT | FCMPX_NUM_BASED,
                     FALSE);
  _assert_comparison(filterx_integer_new((uint64_t)(epoch)), filterx_datetime_new(&ut), FCMPX_GT | FCMPX_NUM_BASED,
                     FALSE);
  _assert_comparison(filterx_integer_new((uint64_t)(epoch)), filterx_datetime_new(&ut), FCMPX_NE | FCMPX_NUM_BASED,
                     FALSE);
}

Test(expr_comparison, test_integer_to_non_numeric_types_num_based_comparison)
{
  // unsupported or unknown types are converted to GenericNumber - NaN
  // NaN is always NE with anything
  _assert_comparison(filterx_integer_new(7), filterx_bytes_new("foobar", 6), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_bytes_new("foobar", 6), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_bytes_new("foobar", 6), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(7), filterx_bytes_new("foobar", 6), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_integer_new(0), filterx_bytes_new("foobar", 6), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_bytes_new("foobar", 6), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_bytes_new("foobar", 6), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_bytes_new("foobar", 6), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
}

Test(expr_comparison, test_double_to_int_num_based_comparison)
{
  _assert_comparison(filterx_double_new(0.0), filterx_integer_new(0), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(3.14), filterx_integer_new(6), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_double_new(6.0), filterx_integer_new(6), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_integer_new(0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_integer_new(7), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(5.0), filterx_integer_new(0), FCMPX_GT | FCMPX_NUM_BASED, TRUE);

  _assert_comparison(filterx_double_new(6.0), filterx_integer_new(6), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_integer_new(0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_integer_new(7), FCMPX_LT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(7.0), filterx_integer_new(0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_double_new(6.0), filterx_integer_new(6), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_integer_new(0), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(6.0), filterx_integer_new(0), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
}

Test(expr_comparison, test_double_to_double_num_based_comparison)
{
  _assert_comparison(filterx_double_new(6.0), filterx_double_new(6.0), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(0.0), filterx_double_new(0.0), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(3.14), filterx_double_new(6.0), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_double_new(6.0), filterx_double_new(6.0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_double_new(0.0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_double_new(7.0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(5.0), filterx_double_new(0.0), FCMPX_GT | FCMPX_NUM_BASED, TRUE);

  _assert_comparison(filterx_double_new(6.0), filterx_double_new(6.0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_double_new(0.0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_double_new(7.0), FCMPX_LT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(7.0), filterx_double_new(0.0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_double_new(6.0), filterx_double_new(6.0), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_double_new(0.0), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(6.0), filterx_double_new(0.0), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
}

Test(expr_comparison, test_double_to_string_num_based_comparison)
{
  _assert_comparison(filterx_double_new(3.0), filterx_string_new("3", 1), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(0.0), filterx_string_new("0", 1), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("6", 1), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("7.1", 3), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("invalid", 5), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("empty", 0), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_double_new(7.0), filterx_string_new("7", 1), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("7.0", 3), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_string_new("7", 1), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(8.0), filterx_string_new("7.1", 3), FCMPX_GT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("invalid", 5), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("empty", 0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_double_new(7.0), filterx_string_new("7", 1), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("7.0", 3), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("0", 1), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("7.1", 3), FCMPX_LT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("invalid", 5), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("empty", 0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_double_new(3.0), filterx_string_new("3", 1), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_string_new("0", 1), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("6", 1), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("7.1", 3), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("invalid", 5), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(7.0), filterx_string_new("empty", 0), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
}

Test(expr_comparison, test_double_to_null_num_based_comparison)
{
  // NULL converted to 0 for some reason
  _assert_comparison(filterx_double_new(7), filterx_null_new(), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7), filterx_null_new(), FCMPX_GT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(7), filterx_null_new(), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7), filterx_null_new(), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(0), filterx_null_new(), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(0), filterx_null_new(), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0), filterx_null_new(), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0), filterx_null_new(), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
}

Test(expr_comparison, test_double_to_bool_num_based_comparison)
{
  _assert_comparison(filterx_double_new(0.0), filterx_boolean_new(false), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(0.0), filterx_boolean_new(false), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_boolean_new(false), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_boolean_new(false), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(1.0), filterx_boolean_new(true), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(1.0), filterx_boolean_new(true), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(1.0), filterx_boolean_new(true), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(1.0), filterx_boolean_new(true), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
}

Test(expr_comparison, test_double_to_datetime_num_based_comparison)
{
  UnixTime ut = { .ut_sec = 1701350398, .ut_usec = 123000, .ut_gmtoff = 3600 };
  guint64 epoch = unix_time_to_unix_epoch(ut);
  _assert_comparison(filterx_double_new((double)(epoch)), filterx_datetime_new(&ut), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new((double)(epoch)), filterx_datetime_new(&ut), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new((double)(epoch)), filterx_datetime_new(&ut), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new((double)(epoch)), filterx_datetime_new(&ut), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
}

Test(expr_comparison, test_double_to_non_numeric_types_num_based_comparison)
{
  // unsupported or unknown types are converted to GenericNumber - NaN
  // NaN is always NE with anything
  _assert_comparison(filterx_double_new(7), filterx_bytes_new("foobar", 6), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7), filterx_bytes_new("foobar", 6), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7), filterx_bytes_new("foobar", 6), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(7), filterx_bytes_new("foobar", 6), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_double_new(0), filterx_bytes_new("foobar", 6), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0), filterx_bytes_new("foobar", 6), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0), filterx_bytes_new("foobar", 6), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_double_new(0), filterx_bytes_new("foobar", 6), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
}

Test(expr_comparison, test_string_to_int_num_based_comparison)
{
  _assert_comparison(filterx_string_new("6.0", 3), filterx_integer_new(6), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_integer_new(0), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("3.1", 3), filterx_integer_new(6), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_string_new("6.0", 3), filterx_integer_new(6), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_integer_new(0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_integer_new(7), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("5.0", 3), filterx_integer_new(0), FCMPX_GT | FCMPX_NUM_BASED, TRUE);

  _assert_comparison(filterx_string_new("6.0", 3), filterx_integer_new(6), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_integer_new(0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_integer_new(7), FCMPX_LT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_integer_new(0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_string_new("6.0", 3), filterx_integer_new(6), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_integer_new(0), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("6.0", 3), filterx_integer_new(0), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
}

Test(expr_comparison, test_string_to_double_num_based_comparison)
{
  _assert_comparison(filterx_string_new("6.0", 3), filterx_double_new(6.0), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_double_new(0.0), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("3.1", 3), filterx_double_new(6.0), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_string_new("6.0", 3), filterx_double_new(6.0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_double_new(0.0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_double_new(7.0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("5.0", 3), filterx_double_new(0.0), FCMPX_GT | FCMPX_NUM_BASED, TRUE);

  _assert_comparison(filterx_string_new("6.0", 3), filterx_double_new(6.0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_double_new(0.0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_double_new(7.0), FCMPX_LT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_double_new(0.0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_string_new("6.0", 3), filterx_double_new(6.0), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_double_new(0.0), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("6.0", 3), filterx_double_new(0.0), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
}

Test(expr_comparison, test_string_to_string_num_based_comparison)
{
  _assert_comparison(filterx_string_new("3.0", 3), filterx_string_new("3", 1), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_string_new("0", 1), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("6", 1), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("7.1", 3), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("invalid", 5), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("empty", 0), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("7", 1), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("7.0", 3), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_string_new("7", 1), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("8.0", 3), filterx_string_new("7.1", 3), FCMPX_GT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("invalid", 5), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("empty", 0), FCMPX_GT | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("7", 1), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("7.0", 3), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("0", 1), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("7.1", 3), FCMPX_LT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("invalid", 5), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("empty", 0), FCMPX_LT | FCMPX_NUM_BASED, FALSE);

  _assert_comparison(filterx_string_new("3.0", 3), filterx_string_new("3", 1), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0.0", 3), filterx_string_new("0", 1), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("6", 1), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("7.1", 3), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("invalid", 5), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("7.0", 3), filterx_string_new("empty", 0), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
}

Test(expr_comparison, test_string_to_null_num_based_comparison)
{
  // NULL converted to 0 for some reason
  _assert_comparison(filterx_string_new("7", 1), filterx_null_new(), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7", 1), filterx_null_new(), FCMPX_GT | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("7", 1), filterx_null_new(), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7", 1), filterx_null_new(), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("0", 1), filterx_null_new(), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("0", 1), filterx_null_new(), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0", 1), filterx_null_new(), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0", 1), filterx_null_new(), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
}

Test(expr_comparison, test_string_to_bool_num_based_comparison)
{
  _assert_comparison(filterx_string_new("0", 1), filterx_boolean_new(false), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("0", 1), filterx_boolean_new(false), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0", 1), filterx_boolean_new(false), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0", 1), filterx_boolean_new(false), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("1", 1), filterx_boolean_new(true), FCMPX_EQ | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("1", 1), filterx_boolean_new(true), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("1", 1), filterx_boolean_new(true), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("1", 1), filterx_boolean_new(true), FCMPX_NE | FCMPX_NUM_BASED, FALSE);
}

Test(expr_comparison, test_string_to_datetime_num_based_comparison)
{
  // the time string can not converted to number, so it returns nan which is not NE in all cases
  // use STRING based comparison between datetime and string
  UnixTime ut = { .ut_sec = 1701350398, .ut_usec = 123000, .ut_gmtoff = 3600 };
  _assert_comparison(filterx_string_new("1701350398.123000+01:00", 23), filterx_datetime_new(&ut),
                     FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("1701350398.123000+01:00", 23), filterx_datetime_new(&ut),
                     FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("1701350398.123000+01:00", 23), filterx_datetime_new(&ut),
                     FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("1701350398.123000+01:00", 23), filterx_datetime_new(&ut),
                     FCMPX_NE | FCMPX_NUM_BASED, TRUE);
}

Test(expr_comparison, test_string_to_non_numeric_types_num_based_comparison)
{
  // unsupported or unknown types are converted to GenericNumber - NaN
  // NaN is always NE with anything
  _assert_comparison(filterx_string_new("7", 1), filterx_bytes_new("foobar", 6), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7", 1), filterx_bytes_new("foobar", 6), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7", 1), filterx_bytes_new("foobar", 6), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("7", 1), filterx_bytes_new("foobar", 6), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
  _assert_comparison(filterx_string_new("0", 1), filterx_bytes_new("foobar", 6), FCMPX_EQ | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0", 1), filterx_bytes_new("foobar", 6), FCMPX_GT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0", 1), filterx_bytes_new("foobar", 6), FCMPX_LT | FCMPX_NUM_BASED, FALSE);
  _assert_comparison(filterx_string_new("0", 1), filterx_bytes_new("foobar", 6), FCMPX_NE | FCMPX_NUM_BASED, TRUE);
}

// FCMPX_STRING_BASED tests

Test(expr_comparison, test_string_to_string_string_based_comparison)
{
  _assert_comparison(filterx_string_new("foobar", 6), filterx_string_new("foobar", 6), FCMPX_EQ | FCMPX_STRING_BASED,
                     TRUE);
  _assert_comparison(filterx_string_new("foobar", 6), filterx_string_new("foobar", 6), FCMPX_LT | FCMPX_STRING_BASED,
                     FALSE);
  _assert_comparison(filterx_string_new("foobar", 6), filterx_string_new("foobar", 6), FCMPX_GT | FCMPX_STRING_BASED,
                     FALSE);
  _assert_comparison(filterx_string_new("foobar", 6), filterx_string_new("foobar", 6), FCMPX_NE | FCMPX_STRING_BASED,
                     FALSE);

  _assert_comparison(filterx_string_new("", 0), filterx_string_new("foobar", 6), FCMPX_EQ | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("", 0), filterx_string_new("foobar", 6), FCMPX_LT | FCMPX_STRING_BASED, TRUE);
  _assert_comparison(filterx_string_new("", 0), filterx_string_new("foobar", 6), FCMPX_GT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("", 0), filterx_string_new("foobar", 6), FCMPX_NE | FCMPX_STRING_BASED, TRUE);

  _assert_comparison(filterx_string_new("foobar", 6), filterx_string_new("", 0), FCMPX_EQ | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("foobar", 6), filterx_string_new("", 0), FCMPX_LT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("foobar", 6), filterx_string_new("", 0), FCMPX_GT | FCMPX_STRING_BASED, TRUE);
  _assert_comparison(filterx_string_new("foobar", 6), filterx_string_new("", 0), FCMPX_NE | FCMPX_STRING_BASED, TRUE);

  _assert_comparison(filterx_string_new("", 0), filterx_string_new("", 0), FCMPX_EQ | FCMPX_STRING_BASED, TRUE);
  _assert_comparison(filterx_string_new("", 0), filterx_string_new("", 0), FCMPX_LT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("", 0), filterx_string_new("", 0), FCMPX_GT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("", 0), filterx_string_new("", 0), FCMPX_NE | FCMPX_STRING_BASED, FALSE);

  _assert_comparison(filterx_string_new("foo", 3), filterx_string_new("alma", 4), FCMPX_EQ | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("foo", 3), filterx_string_new("alma", 4), FCMPX_LT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("foo", 3), filterx_string_new("alma", 4), FCMPX_GT | FCMPX_STRING_BASED, TRUE);
  _assert_comparison(filterx_string_new("foo", 3), filterx_string_new("alma", 4), FCMPX_NE | FCMPX_STRING_BASED, TRUE);
}

Test(expr_comparison, test_string_to_numeric_string_based_comparison)
{
  _assert_comparison(filterx_string_new("3", 1), filterx_integer_new(3), FCMPX_EQ | FCMPX_STRING_BASED, TRUE);
  _assert_comparison(filterx_string_new("3", 1), filterx_integer_new(3), FCMPX_LT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("3", 1), filterx_integer_new(3), FCMPX_GT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("3", 1), filterx_integer_new(3), FCMPX_NE | FCMPX_STRING_BASED, FALSE);

  _assert_comparison(filterx_string_new("3", 1), filterx_double_new(3.0), FCMPX_EQ | FCMPX_STRING_BASED, TRUE);
  _assert_comparison(filterx_string_new("3", 1), filterx_double_new(3.0), FCMPX_LT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("3", 1), filterx_double_new(3.0), FCMPX_GT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("3", 1), filterx_double_new(3.0), FCMPX_NE | FCMPX_STRING_BASED, FALSE);

  _assert_comparison(filterx_string_new("3", 1), filterx_double_new(3.1), FCMPX_EQ | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("3", 1), filterx_double_new(3.1), FCMPX_LT | FCMPX_STRING_BASED,
                     TRUE); // 3.100...001
  _assert_comparison(filterx_string_new("3", 1), filterx_double_new(3.1), FCMPX_GT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("3", 1), filterx_double_new(3.1), FCMPX_NE | FCMPX_STRING_BASED, TRUE);
}

Test(expr_comparison, test_string_to_null_string_based_comparison)
{
  _assert_comparison(filterx_string_new("3", 1), filterx_null_new(), FCMPX_EQ | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("3", 1), filterx_null_new(), FCMPX_LT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("3", 1), filterx_null_new(), FCMPX_GT | FCMPX_STRING_BASED, TRUE);
  _assert_comparison(filterx_string_new("3", 1), filterx_null_new(), FCMPX_NE | FCMPX_STRING_BASED, TRUE);
}

Test(expr_comparison, test_string_to_object_string_based_comparison)
{
  // objects serialized as strings (byte arrays) with default marshaler
  _assert_comparison(filterx_string_new("This is not a valid protobuf array!", 35),
                     filterx_protobuf_new("This is not a valid protobuf array!", 35), FCMPX_EQ | FCMPX_STRING_BASED, TRUE);
  _assert_comparison(filterx_string_new("This is not a valid protobuf array!", 35),
                     filterx_protobuf_new("This is not a valid protobuf array!", 35), FCMPX_LT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("This is not a valid protobuf array!", 35),
                     filterx_protobuf_new("This is not a valid protobuf array!", 35), FCMPX_GT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("This is not a valid protobuf array!", 35),
                     filterx_protobuf_new("This is not a valid protobuf array!", 35), FCMPX_NE | FCMPX_STRING_BASED, FALSE);
}

Test(expr_comparison, test_string_to_boolean_string_based_comparison)
{
  _assert_comparison(filterx_string_new("true", 4), filterx_boolean_new(true), FCMPX_EQ | FCMPX_STRING_BASED, TRUE);
  _assert_comparison(filterx_string_new("true", 4), filterx_boolean_new(true), FCMPX_LT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("true", 4), filterx_boolean_new(true), FCMPX_GT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("true", 4), filterx_boolean_new(true), FCMPX_NE | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("false", 4), filterx_boolean_new(true), FCMPX_EQ | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("false", 4), filterx_boolean_new(true), FCMPX_LT | FCMPX_STRING_BASED, TRUE);
  _assert_comparison(filterx_string_new("false", 4), filterx_boolean_new(true), FCMPX_GT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("false", 4), filterx_boolean_new(true), FCMPX_NE | FCMPX_STRING_BASED, TRUE);
}

Test(expr_comparison, test_string_to_datetime_string_based_comparison)
{
  UnixTime ut = { .ut_sec = 1701350398, .ut_usec = 123000, .ut_gmtoff = 3600 };
  _assert_comparison(filterx_string_new("1701350398.123000+01:00", 23), filterx_datetime_new(&ut),
                     FCMPX_EQ | FCMPX_STRING_BASED, TRUE);
  _assert_comparison(filterx_string_new("1701350398.123000+01:00", 23), filterx_datetime_new(&ut),
                     FCMPX_LT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("1701350398.123000+01:00", 23), filterx_datetime_new(&ut),
                     FCMPX_GT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("1701350398.123000+01:00", 23), filterx_datetime_new(&ut),
                     FCMPX_NE | FCMPX_STRING_BASED, FALSE);

  _assert_comparison(filterx_string_new("1701350398.124000+01:00", 23), filterx_datetime_new(&ut),
                     FCMPX_EQ | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("1701350398.124000+01:00", 23), filterx_datetime_new(&ut),
                     FCMPX_LT | FCMPX_STRING_BASED, FALSE);
  _assert_comparison(filterx_string_new("1701350398.124000+01:00", 23), filterx_datetime_new(&ut),
                     FCMPX_GT | FCMPX_STRING_BASED, TRUE);
  _assert_comparison(filterx_string_new("1701350398.124000+01:00", 23), filterx_datetime_new(&ut),
                     FCMPX_NE | FCMPX_STRING_BASED, TRUE);
}

// FCMPX_TYPE_AWARE tests
// in case of LHS type is one of: [string, bytes, json, protobuf, message_value], uses FCMPX_STRING_BASED comparsion
// in case of any of LHS or RHS is filterx null, compares filterx types (null less than any)
// uses FCMPX_NUM_BASED in any other cases

Test(expr_comparison, test_null_cases_type_aware_comparison)
{
  _assert_comparison(filterx_null_new(), filterx_null_new(), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_null_new(), filterx_null_new(), FCMPX_LT | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_null_new(), filterx_null_new(), FCMPX_GT | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_null_new(), filterx_null_new(), FCMPX_NE | FCMPX_TYPE_AWARE, FALSE);

  _assert_comparison(filterx_null_new(), filterx_integer_new(3), FCMPX_EQ | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_null_new(), filterx_integer_new(3), FCMPX_LT | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_null_new(), filterx_integer_new(3), FCMPX_GT | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_null_new(), filterx_integer_new(3), FCMPX_NE | FCMPX_TYPE_AWARE, TRUE);

  _assert_comparison(filterx_string_new("foobar", 6), filterx_null_new(), FCMPX_EQ | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_string_new("foobar", 6), filterx_null_new(), FCMPX_LT | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_string_new("foobar", 6), filterx_null_new(), FCMPX_GT | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_string_new("foobar", 6), filterx_null_new(), FCMPX_NE | FCMPX_TYPE_AWARE, TRUE);
}

Test(expr_comparison, test_string_cases_type_aware_comparison)
{
  // TODO: fix float precision error in g_ascii_dtostr 3.14 -> "3.1400000000000000000000001"
  // _assert_comparison(filterx_string_new("3.14", 4), filterx_double_new(3.14), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);

  // string - integer
  _assert_comparison(filterx_string_new("443", 3), filterx_integer_new(443), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_string_new("443", 3), filterx_integer_new(443), FCMPX_LT | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_string_new("443", 3), filterx_integer_new(443), FCMPX_GT | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_string_new("443", 3), filterx_integer_new(443), FCMPX_NE  | FCMPX_TYPE_AWARE, FALSE);

  // check if compared as string
  _assert_comparison(filterx_string_new("a443", 4), filterx_integer_new(443), FCMPX_EQ | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_string_new("a443", 4), filterx_integer_new(443), FCMPX_LT | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_string_new("a443", 4), filterx_integer_new(443), FCMPX_GT | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_string_new("a443", 4), filterx_integer_new(443), FCMPX_NE  | FCMPX_TYPE_AWARE, TRUE);

  // bytes - boolean
  _assert_comparison(filterx_bytes_new("true", 4), filterx_boolean_new(true), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_bytes_new("true", 4), filterx_boolean_new(true), FCMPX_LT | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_bytes_new("true", 4), filterx_boolean_new(true), FCMPX_GT | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_bytes_new("true", 4), filterx_boolean_new(true), FCMPX_NE | FCMPX_TYPE_AWARE, FALSE);

  _assert_comparison(filterx_bytes_new("true", 4), filterx_boolean_new(false), FCMPX_EQ | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_bytes_new("true", 4), filterx_boolean_new(false), FCMPX_LT | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_bytes_new("true", 4), filterx_boolean_new(false), FCMPX_GT | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_bytes_new("true", 4), filterx_boolean_new(false), FCMPX_NE | FCMPX_TYPE_AWARE, TRUE);

  // protobuf - double
  _assert_comparison(filterx_protobuf_new("472", 3), filterx_double_new(472.0), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_protobuf_new("472", 3), filterx_double_new(472.0), FCMPX_LT | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_protobuf_new("472", 3), filterx_double_new(472.0), FCMPX_GT | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_protobuf_new("472", 3), filterx_double_new(472.0), FCMPX_NE | FCMPX_TYPE_AWARE, FALSE);

  _assert_comparison(filterx_protobuf_new("a472", 4), filterx_double_new(472.0), FCMPX_EQ | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_protobuf_new("a472", 4), filterx_double_new(472.0), FCMPX_LT | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_protobuf_new("a472", 4), filterx_double_new(472.0), FCMPX_GT | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_protobuf_new("a472", 4), filterx_double_new(472.0), FCMPX_NE | FCMPX_TYPE_AWARE, TRUE);
}

Test(expr_comparison, test_numerical_fallback_type_aware_comparison)
{
  UnixTime ut = { .ut_sec = 1701350398, .ut_usec = 123000, .ut_gmtoff = 3600 };
  guint64 epoch = unix_time_to_unix_epoch(ut);
  _assert_comparison(filterx_integer_new(6), filterx_integer_new(6), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_integer_new(6), filterx_double_new(6.0), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_integer_new(3), filterx_string_new("3", 1), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_integer_new(7), filterx_null_new(), FCMPX_EQ | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_integer_new(0), filterx_boolean_new(false), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_integer_new((uint64_t)(epoch)), filterx_datetime_new(&ut), FCMPX_EQ | FCMPX_TYPE_AWARE,
                     TRUE);
  _assert_comparison(filterx_integer_new(7), filterx_bytes_new("7", 1), FCMPX_EQ | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_double_new(6.0), filterx_integer_new(6), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_double_new(6.0), filterx_double_new(6.0), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_double_new(3.0), filterx_string_new("3", 1), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_double_new(7), filterx_null_new(), FCMPX_EQ | FCMPX_TYPE_AWARE, FALSE);
  _assert_comparison(filterx_double_new(0.0), filterx_boolean_new(false), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_double_new((double)(epoch)), filterx_datetime_new(&ut), FCMPX_EQ | FCMPX_TYPE_AWARE, TRUE);
  _assert_comparison(filterx_double_new(7), filterx_bytes_new("foobar", 6), FCMPX_NE | FCMPX_TYPE_AWARE, TRUE);
}

// // FCMPX_TYPE_AND_VALUE_BASED tests
// // strictly check types first
// // uses FCMPX_TYPE_AWARE when types are matches

Test(expr_comparison, test_nonmatching_types_type_and_value_based_comparison)
{
  UnixTime ut = { .ut_sec = 1701350398, .ut_usec = 123000, .ut_gmtoff = 3600 };
  _assert_comparison(filterx_integer_new(5), filterx_double_new(5), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, FALSE);
  _assert_comparison(filterx_integer_new(5), filterx_boolean_new(true), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, FALSE);
  _assert_comparison(filterx_integer_new(5), filterx_null_new(), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, FALSE);
  _assert_comparison(filterx_integer_new(5), filterx_string_new("5", 1), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, FALSE);
  _assert_comparison(filterx_integer_new(5), filterx_bytes_new("5", 1), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, FALSE);
  _assert_comparison(filterx_integer_new(5), filterx_protobuf_new("5", 1), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, FALSE);
  _assert_comparison(filterx_integer_new(5), filterx_datetime_new(&ut), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, FALSE);
  _assert_comparison(filterx_integer_new(5), filterx_message_value_new("5", 1, LM_VT_BYTES),
                     FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, FALSE);
}

Test(expr_comparison, test_type_aware_fallback_type_and_value_based_comparison)
{
  UnixTime ut = { .ut_sec = 1701350398, .ut_usec = 123000, .ut_gmtoff = 3600 };
  _assert_comparison(filterx_integer_new(5), filterx_integer_new(5), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, TRUE);
  _assert_comparison(filterx_boolean_new(true), filterx_boolean_new(true), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, TRUE);
  _assert_comparison(filterx_null_new(), filterx_null_new(), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, TRUE);
  _assert_comparison(filterx_string_new("5", 1), filterx_string_new("5", 1), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, TRUE);
  _assert_comparison(filterx_bytes_new("5", 1), filterx_bytes_new("5", 1), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, TRUE);
  _assert_comparison(filterx_protobuf_new("5", 1), filterx_protobuf_new("5", 1), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED,
                     TRUE);
  _assert_comparison(filterx_datetime_new(&ut), filterx_datetime_new(&ut), FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, TRUE);
  _assert_comparison(filterx_message_value_new("5", 1, LM_VT_BYTES), filterx_message_value_new("5", 1, LM_VT_BYTES),
                     FCMPX_EQ | FCMPX_TYPE_AND_VALUE_BASED, TRUE);

}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(expr_comparison, .init = setup, .fini = teardown);
