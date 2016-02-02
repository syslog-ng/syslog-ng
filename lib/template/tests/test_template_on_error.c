/*
 * Copyright (c) 2013 Balabit
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

#include <libgen.h>

#include "template/templates.h"
#include "testutils.h"
#include "apphook.h"

#define assert_on_error_parse(on_error,expected)                        \
  do                                                                    \
    {                                                                   \
      gint r;                                                           \
                                                                        \
      assert_true(log_template_on_error_parse(on_error, &r),            \
                  "Parsing '%s' works", on_error);                      \
      assert_gint32(r, expected, "'%s' parses down to '%s'",            \
                    on_error, #expected);                               \
    } while(0)

#define assert_on_error_parse_fails(on_error)                           \
  do                                                                    \
    {                                                                   \
      gint r;                                                           \
                                                                        \
      assert_false(log_template_on_error_parse(on_error, &r),            \
                   "Parsing '%s' works", on_error);                      \
    } while(0)

static void
test_template_on_error(void)
{
  testcase_begin("Testing LogTemplate on-error parsing");

  assert_on_error_parse("drop-message", ON_ERROR_DROP_MESSAGE);
  assert_on_error_parse("silently-drop-message",
                        ON_ERROR_DROP_MESSAGE | ON_ERROR_SILENT);

  assert_on_error_parse("drop-property", ON_ERROR_DROP_PROPERTY);
  assert_on_error_parse("silently-drop-property",
                        ON_ERROR_DROP_PROPERTY | ON_ERROR_SILENT);

  assert_on_error_parse("fallback-to-string", ON_ERROR_FALLBACK_TO_STRING);
  assert_on_error_parse("silently-fallback-to-string",
                        ON_ERROR_FALLBACK_TO_STRING | ON_ERROR_SILENT);

  assert_on_error_parse_fails("do-what-i-mean");

  testcase_end();
}

int
main (void)
{
  app_startup();

  test_template_on_error();

  app_shutdown();

  return 0;
}
