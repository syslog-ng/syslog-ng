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

#include "logmsg/type-hinting.h"
#include "apphook.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef struct _StringHintPair
{
  gchar *string;
  LogMessageValueType value;
} StringHintPair;

typedef struct _StringBoolPair
{
  gchar *string;
  gboolean value;
} StringBoolPair;

typedef struct _StringDoublePair
{
  gchar *string;
  gdouble value;
} StringDoublePair;

typedef struct _StringUInt64Pair
{
  gchar *string;
  guint64 value;
} StringUInt64Pair;


ParameterizedTestParameters(type_hints, test_type_hint_parse)
{
  static StringHintPair string_value_pairs[] =
  {
    {"string",    LM_VT_STRING},
    {"literal",   LM_VT_JSON},
    {"json",      LM_VT_JSON},
    {"boolean",   LM_VT_BOOLEAN},
    {"int",       LM_VT_INTEGER},
    {"int32",     LM_VT_INTEGER},
    {"int64",     LM_VT_INTEGER},
    {"float",     LM_VT_DOUBLE},
    {"double",    LM_VT_DOUBLE},
    {"datetime",  LM_VT_DATETIME},
    {"list",      LM_VT_LIST},
    {"null",      LM_VT_NULL},
    {"bytes",     LM_VT_BYTES},
    {"protobuf",  LM_VT_PROTOBUF},
  };

  return cr_make_param_array(StringHintPair, string_value_pairs,
                             sizeof(string_value_pairs) / sizeof(string_value_pairs[0]));
}

ParameterizedTest(StringHintPair *string_value_pair, type_hints, test_type_hint_parse)
{
  LogMessageValueType type_hint;
  GError *error = NULL;

  cr_assert_eq(type_hint_parse(string_value_pair->string, &type_hint, &error), TRUE,
               "Parsing \"%s\" as type hint failed", string_value_pair->string);
  cr_assert_eq(type_hint, string_value_pair->value);
  cr_assert_null(error);
}

Test(type_hints, test_invalid_type_hint_parse)
{
  LogMessageValueType t;
  GError *error = NULL;
  cr_assert_eq(type_hint_parse("invalid-hint", &t, &error), FALSE,
               "Parsing an invalid hint results in an error.");

  cr_assert_not_null(error);
  cr_assert_eq(error->domain, TYPE_HINTING_ERROR);
  cr_assert_eq(error->code, TYPE_HINTING_INVALID_TYPE);
  g_clear_error(&error);
}

ParameterizedTestParameters(type_hints, test_bool_cast)
{
  static StringBoolPair string_value_pairs[] =
  {
    {"True",  TRUE},
    {"true",  TRUE},
    {"1",     TRUE},
    {"totaly true", TRUE},
    {"False", FALSE},
    {"false", FALSE},
    {"0",     FALSE},
    {"fatally false", FALSE}
  };

  return cr_make_param_array(StringBoolPair, string_value_pairs,
                             sizeof(string_value_pairs) / sizeof(string_value_pairs[0]));
}

ParameterizedTest(StringBoolPair *string_value_pair, type_hints, test_bool_cast)
{
  gboolean value;
  GError *error = NULL;

  cr_assert_eq(type_cast_to_boolean(string_value_pair->string, -1, &value, &error), TRUE,
               "Type cast of \"%s\" to gboolean failed", string_value_pair->string);
  cr_assert_eq(value, string_value_pair->value);
  cr_assert_null(error);
}

Test(type_hints, test_invalid_bool_cast)
{
  GError *error = NULL;
  gboolean value;

  /* test invalid boolean value cast */
  cr_assert_eq(type_cast_to_boolean("booyah", -1, &value, &error), FALSE,
               "Type cast \"booyah\" to gboolean should be failed");
  cr_assert_not_null(error);
  cr_assert_eq(error->domain, TYPE_HINTING_ERROR);
  cr_assert_eq(error->code, TYPE_HINTING_INVALID_CAST);

  g_clear_error(&error);
}

Test(type_hints, test_int32_cast)
{
  GError *error = NULL;
  gint32 value;

  cr_assert(type_cast_to_int32("12345", -1, &value, &error), "Type cast of \"12345\" to gint32 failed");
  cr_assert_eq(value, 12345);
  cr_assert_null(error);

  cr_assert(type_cast_to_int32("0x1000", -1, &value, &error), "Type cast of \"0x1000\" to gint32 failed");
  cr_assert_eq(value, 0x1000);
  cr_assert_null(error);

  cr_assert(type_cast_to_int32("0111", -1, &value, &error), "Type cast of \"0111\" to gint32 failed");
  cr_assert_eq(value, 111);
  cr_assert_null(error);

  /* test for invalid string */
  cr_assert_not(type_cast_to_int32("12345a", -1, &value, &error),
                "Type cast of invalid string to gint32 should be failed");
  cr_assert_not_null(error);
  cr_assert_eq(error->domain, TYPE_HINTING_ERROR);
  cr_assert_eq(error->code, TYPE_HINTING_INVALID_CAST);
  g_clear_error(&error);

  /* empty string */
  cr_assert_not(type_cast_to_int32("", -1, &value, &error),
                "Type cast of empty string to gint32 should be failed");
  cr_assert_not_null(error);
  cr_assert_eq(error->domain, TYPE_HINTING_ERROR);
  cr_assert_eq(error->code, TYPE_HINTING_INVALID_CAST);

  g_clear_error(&error);
}

Test(type_hints, test_int32_nonzero_terminated)
{
  GError *error = NULL;
  gint32 int32_value;
  gint64 int64_value;
  gboolean bool_value;
  gdouble dbl_value;
  UnixTime ut_value;

  cr_assert(type_cast_to_int32("12345", 3, &int32_value, &error),
            "Type cast of non-zero terminated \"123\" to gint32 failed");
  cr_assert_eq(int32_value, 123);
  cr_assert_null(error);

  cr_assert(type_cast_to_int64("12345", 3, &int64_value, &error),
            "Type cast of non-zero terminated \"123\" to gint64 failed");
  cr_assert_eq(int64_value, 123);
  cr_assert_null(error);

  cr_assert(type_cast_to_boolean("12", 1, &bool_value, &error),
            "Type cast of non-zero terminated \"1\" to gboolean failed");
  cr_assert_eq(bool_value, 1);
  cr_assert_null(error);

  cr_assert(type_cast_to_double("123456", 3, &dbl_value, &error),
            "Type cast of non-zero terminated \"123\" to gdouble failed");
  cr_assert(dbl_value - 123 < 0.1);
  cr_assert_null(error);

  cr_assert(type_cast_to_datetime_unixtime("1699134067.123", 12, &ut_value, &error),
            "Type cast of non-zero terminated \"1699134067.1\" to unixtime failed");
  cr_assert(ut_value.ut_sec == 1699134067);
  cr_assert(ut_value.ut_usec == 100000);
  cr_assert_null(error);
}

Test(type_hints, test_int64_cast)
{
  GError *error = NULL;
  gint64 value;

  cr_assert(type_cast_to_int64("12345", -1, &value, &error), "Type cast of \"12345\" to gint64 failed");
  cr_assert_eq(value, 12345);
  cr_assert_null(error);

  cr_assert(type_cast_to_int64("0x1000", -1, &value, &error), "Type cast of \"0x1000\" to gint64 failed");
  cr_assert_eq(value, 0x1000);
  cr_assert_null(error);

  cr_assert(type_cast_to_int64("0111", -1, &value, &error), "Type cast of \"0111\" to gint64 failed");
  cr_assert_eq(value, 111);
  cr_assert_null(error);


  /* test for invalid string */
  cr_assert_not(type_cast_to_int64("12345a", -1, &value, &error),
                "Type cast of invalid string to gint64 should be failed");
  cr_assert_not_null(error);
  cr_assert_eq(error->domain, TYPE_HINTING_ERROR);
  cr_assert_eq(error->code, TYPE_HINTING_INVALID_CAST);
  g_clear_error(&error);

  /* empty string */
  cr_assert_not(type_cast_to_int64("", -1, &value, &error),
                "Type cast of empty string to gint64 should be failed");
  cr_assert_not_null(error);
  cr_assert_eq(error->domain, TYPE_HINTING_ERROR);
  cr_assert_eq(error->code, TYPE_HINTING_INVALID_CAST);
  g_clear_error(&error);
}

ParameterizedTestParameters(type_hints, test_double_cast)
{
  static StringDoublePair string_value_pairs[] =
  {
#ifdef INFINITY
    {"INF", (gdouble)INFINITY},
#endif
    {"1.0", 1.0},
    {"1e-100000000", 0.0}
  };

  return cr_make_param_array(StringDoublePair, string_value_pairs,
                             sizeof(string_value_pairs) / sizeof(string_value_pairs[0]));
}

static void
cr_assert_gdouble_eq(gdouble a, gdouble b)
{
  const gint is_a_inf = isinf(a);
  const gint is_b_inf = isinf(b);

  cr_assert_eq(is_a_inf, is_b_inf);
  if (is_a_inf && is_b_inf)
    {
      cr_assert(TRUE);
      return;
    }

  cr_assert(fabs(a-b) < G_MINDOUBLE);
}

ParameterizedTest(StringDoublePair *string_value_pair, type_hints, test_double_cast)
{
  gdouble value;
  GError *error = NULL;

  cr_assert(type_cast_to_double(string_value_pair->string, -1, &value, &error),
            "Type cast of \"%s\" to double failed", string_value_pair->string);

  cr_assert_gdouble_eq(value, string_value_pair->value);
  cr_assert_null(error);
}

ParameterizedTestParameters(type_hints, test_invalid_double_cast)
{
  static StringDoublePair string_list[] =
  {
    {"2.0bad", },
    {"bad", },
    {"", },
    {"1e1000000", },
    {"-1e1000000" },
  };

  return cr_make_param_array(StringDoublePair, string_list, sizeof(string_list) / sizeof(string_list[0]));
}

ParameterizedTest(StringDoublePair *string_value_pair, type_hints, test_invalid_double_cast)
{
  gdouble value;
  GError *error = NULL;

  cr_assert_not(type_cast_to_double(string_value_pair->string, -1, &value, &error),
                "Type cast of invalid string (%s) to double should be failed", string_value_pair->string);
  cr_assert_not_null(error);
  cr_assert_eq(error->domain, TYPE_HINTING_ERROR);
  cr_assert_eq(error->code, TYPE_HINTING_INVALID_CAST);
  g_clear_error(&error);
}

ParameterizedTestParameters(type_hints, test_datetime_cast)
{
  static StringUInt64Pair string_value_pairs[] =
  {
    {"12345",   12345000},
    {"12345.5", 12345500},
    {"12345.54", 12345540},
    {"12345.543", 12345543},
    {"12345.54321", 12345543},
    {"12345.987654", 12345987},
    {"12345.987654321", 12345987},
    {"12345+05:00", 12345000},
    {"12345-05:00", 12345000},
  };

  return cr_make_param_array(StringUInt64Pair, string_value_pairs,
                             sizeof(string_value_pairs) / sizeof(string_value_pairs[0]));
}

ParameterizedTest(StringUInt64Pair *string_value_pair, type_hints, test_datetime_cast)
{
  gint64 value;
  GError *error = NULL;

  cr_assert_eq(type_cast_to_datetime_msec(string_value_pair->string, -1, &value, &error), TRUE,
               "Type cast of \"%s\" to msecs failed", string_value_pair->string);
  cr_assert_eq(value, string_value_pair->value,
               "datetime cast failed %" G_GINT64_FORMAT " != %" G_GINT64_FORMAT,
               value, string_value_pair->value);
  cr_assert_null(error);
}

ParameterizedTestParameters(type_hints, test_invalid_datetime_cast)
{
  static StringUInt64Pair string_value_pairs[] =
  {
    {"invalid", },
    {"12345T", },
    {"12345.", },
    {"12345.1234567890", },
    {"12345+XX:YY", 12345000},
    {"12345-05", 12345000},
    {"12345-XX:YY", 12345000}

  };

  return cr_make_param_array(StringUInt64Pair, string_value_pairs,
                             sizeof(string_value_pairs) / sizeof(string_value_pairs[0]));
}


ParameterizedTest(StringUInt64Pair *string_value_pair, type_hints, test_invalid_datetime_cast)
{
  GError *error = NULL;
  gint64 value;

  cr_assert_eq(type_cast_to_datetime_msec(string_value_pair->string, -1, &value, &error), FALSE,
               "Type cast of invalid string to gint64 should have failed %s", string_value_pair->string);
  cr_assert_not_null(error);
  cr_assert_eq(error->domain, TYPE_HINTING_ERROR);
  cr_assert_eq(error->code, TYPE_HINTING_INVALID_CAST);
  g_clear_error(&error);
}
