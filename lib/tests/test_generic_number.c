/*
 * Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
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

#include "generic-number.h"
#include <math.h>

Test(generic_number, test_set_signed_int_retrieves_the_value_specified)
{
  GenericNumber gn;

  gn_set_int64(&gn, 1);
  cr_assert_eq(gn_as_int64(&gn), 1);

  gn_set_int64(&gn, -1);
  cr_assert_eq(gn_as_int64(&gn), -1);

  gn_set_int64(&gn, G_MAXINT64);
  cr_assert_eq(gn_as_int64(&gn), G_MAXINT64);

  gn_set_int64(&gn, G_MININT64);
  cr_assert_eq(gn_as_int64(&gn), G_MININT64);
}

Test(generic_number, test_set_double_retrieves_the_value_specified)
{
  GenericNumber gn;

  gn_set_double(&gn, 1.0, -1);
  cr_assert_float_eq(gn_as_double(&gn), 1.0, DBL_EPSILON);

  gn_set_double(&gn, -1.0, -1);
  cr_assert_float_eq(gn_as_double(&gn), -1.0, DBL_EPSILON);
}

Test(generic_number, test_integer_is_converted_to_double)
{
  GenericNumber gn;

  gn_set_int64(&gn, -5000);
  cr_assert_float_eq(gn_as_double(&gn), -5000.0, DBL_EPSILON);
}

Test(generic_number, test_double_is_converted_to_integer_by_rounding)
{
  GenericNumber gn;

  gn_set_double(&gn, 1.5, -1);
  cr_assert_eq(gn_as_int64(&gn), 2);

  gn_set_double(&gn, -1.5, -1);
  cr_assert_eq(gn_as_int64(&gn), -2);
}

Test(generic_number, test_double_outside_of_the_int64_range_is_represented_as_extremal_values)
{
  GenericNumber gn;

  gn_set_double(&gn, ((gdouble) G_MAXINT64) + 1.0, -1);
  cr_assert_eq(gn_as_int64(&gn), G_MAXINT64);

  gn_set_double(&gn, ((gdouble) G_MAXINT64) + 1e6, -1);
  cr_assert_eq(gn_as_int64(&gn), G_MAXINT64);

  gn_set_double(&gn, ((gdouble) G_MININT64) - 1e6, -1);
  cr_assert_eq(gn_as_int64(&gn), G_MININT64);

  gn_set_double(&gn, ((gdouble) G_MININT64) - 1.0, -1);
  cr_assert_eq(gn_as_int64(&gn), G_MININT64);
}

Test(generic_number, test_set_nan_becomes_a_nan)
{
  GenericNumber gn;

  gn_set_nan(&gn);
  cr_assert(gn_is_nan(&gn));

  /* NAN requires _GNU_SOURCE */
  gn_set_double(&gn, (gdouble) NAN, -1);
  cr_assert(gn_is_nan(&gn));
}

Test(generic_number, test_nan_operation_is_zero_triggers_an_abort, .signal=SIGABRT)
{
  GenericNumber gn;
  gn_set_nan(&gn);
  gn_is_zero(&gn);

  cr_assert(FALSE, "Should not be reached");
}

Test(generic_number, test_nan_operation_compare_triggers_an_abort, .signal=SIGABRT)
{
  GenericNumber gn1, gn2;
  gn_set_nan(&gn1);
  gn_set_double(&gn2, 0.0, -1);

  gn_compare(&gn1, &gn2);

  cr_assert(FALSE, "Should not be reached");
}
