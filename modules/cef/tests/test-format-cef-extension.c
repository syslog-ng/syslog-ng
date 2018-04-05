/*
 * Copyright (c) 2015-2018 Balabit
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
 */

#include <criterion/criterion.h>

#include "libtest/cr_template.h"
#include "apphook.h"
#include "plugin.h"
#include "cfg.h"
#include "logmsg/logmsg.h"

#include <stdarg.h>

#define _EXPECT_CEF_RESULT(...) _expect_cef_result_va(__VA_ARGS__, NULL)
#define _EXPECT_DROP_MESSAGE(...) _expect_cef_result_va("", __VA_ARGS__, NULL)
#define _EXPECT_SKIP_BAD_PROPERTY(...) _expect_skip_bad_property_va(__VA_ARGS__, NULL)
#define _EXPECT_CEF_RESULT_FORMAT(X, ...) _expect_cef_result_format_va(X, __VA_ARGS__, NULL);

static void
_expect_cef_result_properties_list(const gchar *expected, va_list ap)
{
  LogMessage *msg = message_from_list(ap);

  assert_template_format_msg("$(format-cef-extension --subkeys .cef.)", expected, msg);
  log_msg_unref(msg);
}

static void
_expect_cef_result_va(const gchar *expected, ...)
{
  va_list ap;
  configuration->template_options.on_error = ON_ERROR_DROP_MESSAGE | ON_ERROR_SILENT;

  va_start(ap, expected);
  _expect_cef_result_properties_list(expected, ap);
  va_end(ap);
}

static void
_expect_skip_bad_property_va(const gchar *expected, ...)
{
  va_list ap;
  configuration->template_options.on_error = ON_ERROR_DROP_PROPERTY | ON_ERROR_SILENT;

  va_start(ap, expected);
  _expect_cef_result_properties_list(expected, ap);
  va_end(ap);
}

static void
_expect_cef_result_format_va(const gchar *format, const gchar *expected, ...)
{
  va_list ap;
  va_start(ap, expected);
  LogMessage *msg = message_from_list(ap);
  va_end(ap);

  configuration->template_options.on_error = ON_ERROR_DROP_MESSAGE | ON_ERROR_SILENT;
  assert_template_format_msg(format, expected, msg);
  log_msg_unref(msg);
}


void
setup(void)
{
  app_startup();
  setenv("TZ", "UTC", TRUE);
  tzset();
  init_template_tests();
  cfg_load_module(configuration, "cef");
}

void
teardown(void)
{
  deinit_template_tests();
  app_shutdown();
}

TestSuite(format_cef, .init = setup, .fini = teardown);

Test(format_cef, test_null_in_value)
{
  LogMessage *msg = create_empty_message();

  configuration->template_options.on_error = ON_ERROR_DROP_MESSAGE | ON_ERROR_SILENT;
  log_msg_set_value_by_name(msg, ".cef.k", "a\0b", 3);
  assert_template_format_msg("$(format-cef-extension --subkeys .cef.)", "k=a\\u0000b", msg);
  log_msg_unref(msg);
}

Test(format_cef, test_filter)
{
  _EXPECT_CEF_RESULT("k=v", ".cef.k", "v", "x", "w");
}

Test(format_cef, test_multiple_properties_with_space)
{
  _EXPECT_CEF_RESULT("act=c:/program files dst=10.0.0.1",
                     ".cef.act", "c:/program files",
                     ".cef.dst", "10.0.0.1");
}

Test(format_cef, test_multiple_properties)
{
  _EXPECT_CEF_RESULT("k=v x=y",
                     ".cef.k", "v",
                     ".cef.x", "y");
}

Test(format_cef, test_drop_property)
{
  _EXPECT_SKIP_BAD_PROPERTY("kkk=v",
                            ".cef.a|b", "c",
                            ".cef.kkk", "v",
                            ".cef.x=y", "w");
}

Test(format_cef, test_drop_message)
{
  _EXPECT_DROP_MESSAGE(".cef.a|b", "c",
                       ".cef.kkk", "v",
                       ".cef.x=y", "w");
}

Test(format_cef, test_empty)
{
  _EXPECT_CEF_RESULT("");
}

Test(format_cef, test_inline)
{
  _EXPECT_CEF_RESULT_FORMAT("$(format-cef-extension --subkeys .cef. .cef.k=v)", "k=v");
}

Test(format_cef, test_space)
{
  _EXPECT_CEF_RESULT("act=blocked a ping", ".cef.act", "blocked a ping");
}

Test(format_cef, test_charset)
{
  _EXPECT_DROP_MESSAGE(".cef.árvíztűrőtükörfúrógép", "v");
  _EXPECT_CEF_RESULT("k=árvíztűrőtükörfúrógép", ".cef.k", "árvíztűrőtükörfúrógép");

  _EXPECT_CEF_RESULT("k=\\xff", ".cef.k", "\xff");
  _EXPECT_CEF_RESULT("k=\\xc3", ".cef.k", "\xc3");
  _EXPECT_DROP_MESSAGE(".cef.k\xff", "v");
  _EXPECT_DROP_MESSAGE(".cef.k\xc3", "v");
}

Test(format_cef, test_escaping)
{
  _EXPECT_CEF_RESULT("act=\\\\", ".cef.act", "\\");
  _EXPECT_CEF_RESULT("act=\\\\\\\\", ".cef.act", "\\\\");
  _EXPECT_CEF_RESULT("act=\\=", ".cef.act", "=");
  _EXPECT_CEF_RESULT("act=|", ".cef.act", "|");
  _EXPECT_CEF_RESULT("act=\\u0009", ".cef.act", "\t");
  _EXPECT_CEF_RESULT("act=\\n", ".cef.act", "\n");
  _EXPECT_CEF_RESULT("act=\\r", ".cef.act", "\r");
  _EXPECT_CEF_RESULT("act=v\\n", ".cef.act", "v\n");
  _EXPECT_CEF_RESULT("act=v\\r", ".cef.act", "v\r");
  _EXPECT_CEF_RESULT("act=u\\nv", ".cef.act", "u\nv");
  _EXPECT_CEF_RESULT("act=\\r\\n", ".cef.act", "\r\n");
  _EXPECT_CEF_RESULT("act=\\n\\r", ".cef.act", "\n\r");
  _EXPECT_CEF_RESULT("act=this is a long value \\= something",
                     ".cef.act", "this is a long value = something");

  _EXPECT_DROP_MESSAGE(".cef.k=w", "v");
  _EXPECT_DROP_MESSAGE(".cef.k|w", "v");
  _EXPECT_DROP_MESSAGE(".cef.k\\w", "v");
  _EXPECT_DROP_MESSAGE(".cef.k\nw", "v");
  _EXPECT_DROP_MESSAGE(".cef.k w", "v");
}

Test(format_cef, test_prefix)
{
  configuration->template_options.on_error = ON_ERROR_DROP_MESSAGE | ON_ERROR_SILENT;

  _EXPECT_CEF_RESULT_FORMAT("$(format-cef-extension --subkeys ..)", "k=v", "..k", "v");
  _EXPECT_CEF_RESULT_FORMAT("$(format-cef-extension --subkeys ,)", "k=v", ",k", "v");
  _EXPECT_CEF_RESULT_FORMAT("$(format-cef-extension --subkeys .cef.)", "", "k", "v");
  _EXPECT_CEF_RESULT_FORMAT("$(format-cef-extension --subkeys ' ')", "k=v", " k", "v");
  _EXPECT_CEF_RESULT_FORMAT("$(format-cef-extension --subkeys \" \")", "k=v", " k", "v");

  _EXPECT_CEF_RESULT_FORMAT("$(format-cef-extension x=y)", "x=y", "k", "v");
  _EXPECT_CEF_RESULT_FORMAT("$(format-cef-extension)", "", "k", "v");

  assert_template_failure("$(format-cef-extension --subkeys)",
                          "Missing argument for --subkeys");
  assert_template_failure("$(format-cef-extension --subkeys '')",
                          "Error parsing value-pairs: --subkeys requires a non-empty argument");
  assert_template_failure("$(format-cef-extension --subkeys \"\")",
                          "Error parsing value-pairs: --subkeys requires a non-empty argument");
}

Test(format_cef, test_macro_parity)
{
  _EXPECT_CEF_RESULT("", "k");
  _EXPECT_CEF_RESULT_FORMAT("", "");
  _EXPECT_CEF_RESULT_FORMAT("", "", "k");
  _EXPECT_DROP_MESSAGE("");
  _EXPECT_DROP_MESSAGE("", "k");
  _EXPECT_SKIP_BAD_PROPERTY("");
  _EXPECT_SKIP_BAD_PROPERTY("", "k");
}
