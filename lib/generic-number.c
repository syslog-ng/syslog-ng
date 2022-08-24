/*
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
 */
#include "generic-number.h"
#include <math.h>

gdouble
gn_as_double(const GenericNumber *number)
{
  if (number->type == GN_DOUBLE)
    return number->value.raw_double;
  else if (number->type == GN_INT64)
    return (gdouble) number->value.raw_int64;
  g_assert_not_reached();
}

void
gn_set_double(GenericNumber *number, gdouble value, gint precision)
{
  number->type = GN_DOUBLE;
  number->value.raw_double = value;
  number->precision = precision > 0 ? precision : 20;
}

gint64
gn_as_int64(const GenericNumber *number)
{
  if (number->type == GN_DOUBLE)
    {
      double r = round(number->value.raw_double);

      if (r <= (double) G_MININT64)
        return G_MININT64;
      if (r >= (double) G_MAXINT64)
        return G_MAXINT64;
      return (gint64) r;
    }
  else if (number->type == GN_INT64)
    return number->value.raw_int64;
  g_assert_not_reached();
}

void
gn_set_int64(GenericNumber *number, gint64 value)
{
  number->type = GN_INT64;
  number->value.raw_int64 = value;
  number->precision = 0;
}

gboolean
gn_is_zero(const GenericNumber *number)
{
  if (number->type == GN_INT64)
    return number->value.raw_int64 == 0;

  if (number->type == GN_DOUBLE)
    return fabs(number->value.raw_double) < DBL_EPSILON;

  g_assert_not_reached();
}

void
gn_set_nan(GenericNumber *number)
{
  number->type = GN_NAN;
}

gboolean
gn_is_nan(const GenericNumber *number)
{
  return number->type == GN_NAN || (number->type == GN_DOUBLE && isnan(number->value.raw_double));
}

static gint
_compare_int64(gint64 l, gint64 r)
{
  if (l == r)
    return 0;
  else if (l < r)
    return -1;

  return 1;
}

static gint
_compare_double(gdouble l, gdouble r)
{
  if (fabs(l - r) < DBL_EPSILON)
    return 0;
  else if (l < r)
    return -1;

  return 1;
}

gint
gn_compare(const GenericNumber *left, const GenericNumber *right)
{
  if (left->type == right->type)
    {
      if (left->type == GN_INT64)
        return _compare_int64(gn_as_int64(left), gn_as_int64(right));
      else if (left->type == GN_DOUBLE)
        return _compare_double(gn_as_double(left), gn_as_double(right));
    }
  else if (left->type == GN_NAN || right->type == GN_NAN)
    {
      ;
    }
  else if (left->type == GN_DOUBLE || right->type == GN_DOUBLE)
    {
      return _compare_double(gn_as_double(left), gn_as_double(right));
    }
  else
    {
      return _compare_int64(gn_as_int64(left), gn_as_int64(right));
    }
  /* NaNs cannot be compared */
  g_assert_not_reached();
}
