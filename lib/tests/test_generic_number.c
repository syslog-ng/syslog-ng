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
  gn_set_double(&gn, NAN, -1);
  cr_assert(gn_is_nan(&gn));
}
