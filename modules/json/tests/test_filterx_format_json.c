/*
 * Copyright (c) 2024 Attila Szakacs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <criterion/criterion.h>
#include "libtest/cr_template.h"
#include "libtest/filterx-lib.h"

#include "filterx-format-json.h"
#include "filterx/object-primitive.h"
#include "filterx/object-null.h"
#include "filterx/object-string.h"
#include "filterx/object-json.h"
#include "filterx/object-message-value.h"
#include "filterx/object-list-interface.h"

#include "apphook.h"
#include "scratch-buffers.h"
#include "plugin.h"
#include "cfg.h"
#include "logmsg/logmsg.h"

static FilterXObject *
_exec_format_json_and_unref(FilterXObject *arg)
{
  GPtrArray *args = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  g_ptr_array_add(args, arg);

  FilterXObject *result = filterx_format_json_new(args);
  cr_assert(filterx_object_is_type(result, &FILTERX_TYPE_NAME(string)));

  g_ptr_array_unref(args);
  return result;
}

static void
_assert_filterx_format_json_and_unref(FilterXObject *arg, const gchar *expected_json)
{
  FilterXObject *result = _exec_format_json_and_unref(arg);
  cr_assert_str_eq(filterx_string_get_value(result, NULL), expected_json, "%s", filterx_string_get_value(result, NULL));
  filterx_object_unref(result);
}

static void
_assert_filterx_format_json_double_and_unref(FilterXObject *arg, gdouble expected_double)
{
  FilterXObject *result = _exec_format_json_and_unref(arg);
  cr_assert_float_eq(g_strtod(filterx_string_get_value(result, NULL), NULL), expected_double, DBL_EPSILON);
  filterx_object_unref(result);
}

Test(filterx_format_json, test_filterx_format_json)
{
  /* boolean */
  _assert_filterx_format_json_and_unref(filterx_boolean_new(TRUE), "true");
  _assert_filterx_format_json_and_unref(filterx_boolean_new(FALSE), "false");

  /* null */
  _assert_filterx_format_json_and_unref(filterx_null_new(), "null");

  /* integer */
  _assert_filterx_format_json_and_unref(filterx_integer_new(G_MAXINT64), "9223372036854775807");
  _assert_filterx_format_json_and_unref(filterx_integer_new(G_MININT64), "-9223372036854775808");

  /* double */
  _assert_filterx_format_json_double_and_unref(filterx_double_new(-13.37), -13.37);

  /* string */
  _assert_filterx_format_json_and_unref(filterx_string_new("foo", -1), "\"foo\"");
  _assert_filterx_format_json_and_unref(filterx_string_new("árvíztűrőtükörfúrógép", -1),
                                        "\"árvíztűrőtükörfúrógép\"");
  _assert_filterx_format_json_and_unref(filterx_string_new("inner\"quote", -1), "\"inner\\\"quote\"");
  _assert_filterx_format_json_and_unref(filterx_string_new("\xc2\xbf \xc2\xb6 \xc2\xa9 \xc2\xb1", -1),
                                        "\"\xc2\xbf \xc2\xb6 \xc2\xa9 \xc2\xb1\""); // ¿ ¶ © ±
  _assert_filterx_format_json_and_unref(filterx_string_new("\xc3\x88 \xc3\x90", -1), "\"\xc3\x88 \xc3\x90\""); // È Ð
  _assert_filterx_format_json_and_unref(filterx_string_new("\x07\x09", -1), "\"\\u0007\\t\"");

  /* bytes */
  _assert_filterx_format_json_and_unref(filterx_bytes_new("binary stuff follows \"\xad árvíztűrőtükörfúrógép",
                                                          54),
                                        "\"YmluYXJ5IHN0dWZmIGZvbGxvd3MgIq0gw6FydsOtenTFsXLFkXTDvGvDtnJmw7pyw7Nnw6lw\"");
  _assert_filterx_format_json_and_unref(filterx_bytes_new("\xc3", 1), "\"ww==\"");
  _assert_filterx_format_json_and_unref(filterx_bytes_new("binary\0stuff", 12), "\"YmluYXJ5AHN0dWZm\"");

  /* protobuf */
  _assert_filterx_format_json_and_unref(filterx_protobuf_new("\4\5\6\7", 4), "\"BAUGBw==\"");

  /* json_object */
  _assert_filterx_format_json_and_unref(filterx_json_object_new_from_repr("{\"foo\": 42}", -1), "{\"foo\":42}");

  /* json_array */
  _assert_filterx_format_json_and_unref(filterx_json_array_new_from_repr("[\"foo\", 42]", -1), "[\"foo\",42]");

  /* message_value */
  _assert_filterx_format_json_and_unref(filterx_message_value_new("T", -1, LM_VT_BOOLEAN), "true");
  _assert_filterx_format_json_and_unref(filterx_message_value_new("F", -1, LM_VT_BOOLEAN), "false");
  _assert_filterx_format_json_and_unref(filterx_message_value_new("", -1, LM_VT_NULL), "null");
  _assert_filterx_format_json_and_unref(filterx_message_value_new("9223372036854775807", -1, LM_VT_INTEGER),
                                        "9223372036854775807");
  _assert_filterx_format_json_and_unref(filterx_message_value_new("-9223372036854775808", -1, LM_VT_INTEGER),
                                        "-9223372036854775808");
  _assert_filterx_format_json_double_and_unref(filterx_message_value_new("-13.37", -1, LM_VT_DOUBLE), -13.37);
  _assert_filterx_format_json_and_unref(filterx_message_value_new("inner\"quote", -1, LM_VT_STRING),
                                        "\"inner\\\"quote\"");
  _assert_filterx_format_json_and_unref(filterx_message_value_new("binary\0stuff", 12, LM_VT_BYTES),
                                        "\"YmluYXJ5AHN0dWZm\"");
  _assert_filterx_format_json_and_unref(filterx_message_value_new("\4\5\6\7", 4, LM_VT_PROTOBUF), "\"BAUGBw==\"");
  _assert_filterx_format_json_and_unref(filterx_message_value_new("{\"foo\" : 42}", -1, LM_VT_JSON), "{\"foo\" : 42}");
  _assert_filterx_format_json_and_unref(filterx_message_value_new("[\"foo\" , 42]", -1, LM_VT_JSON), "[\"foo\" , 42]");

  /* unknown type */
  GString *repr = g_string_new(NULL);
  g_string_printf(repr, "\"%s\"", filterx_test_unknown_object_repr(NULL));
  _assert_filterx_format_json_and_unref(filterx_test_unknown_object_new(), repr->str);
  g_string_free(repr, TRUE);

  FilterXObject *foo = filterx_string_new("foo", -1);
  FilterXObject *bar = filterx_string_new("bar", -1);
  FilterXObject *fx_42 = filterx_integer_new(42);
  FilterXObject *fx_1337 = filterx_integer_new(1337);

  /* dict */
  FilterXObject *dict = filterx_test_dict_new();
  cr_assert(filterx_object_set_subscript(dict, foo, &fx_42));
  cr_assert(filterx_object_set_subscript(dict, bar, &fx_1337));
  _assert_filterx_format_json_and_unref(dict, "{\"foo\":42,\"bar\":1337}");

  /* list */
  FilterXObject *list = filterx_test_list_new();
  cr_assert(filterx_list_append(list, &foo));
  cr_assert(filterx_list_append(list, &bar));
  _assert_filterx_format_json_and_unref(list, "[\"foo\",\"bar\"]");

  filterx_object_unref(foo);
  filterx_object_unref(bar);
  filterx_object_unref(fx_42);
  filterx_object_unref(fx_1337);
}

void
setup(void)
{
  app_startup();
  init_template_tests();
  init_libtest_filterx();
  cfg_load_module(configuration, "json-plugin");
}

void
teardown(void)
{
  scratch_buffers_explicit_gc();
  deinit_libtest_filterx();
  deinit_template_tests();
  app_shutdown();
}

TestSuite(filterx_format_json, .init = setup, .fini = teardown);
