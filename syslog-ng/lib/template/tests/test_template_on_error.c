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

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include "apphook.h"

typedef struct _TemplateTestCase
{
  const gchar *actual;
  gint expected;
} TemplateTestCase;

ParameterizedTestParameters(template_on_err, test_success)
{
  static TemplateTestCase test_cases[] =
  {
    {"drop-message", ON_ERROR_DROP_MESSAGE},
    {"silently-drop-message", ON_ERROR_DROP_MESSAGE | ON_ERROR_SILENT},
    {"drop-property", ON_ERROR_DROP_PROPERTY},
    {"silently-drop-property", ON_ERROR_DROP_PROPERTY | ON_ERROR_SILENT},
    {"fallback-to-string", ON_ERROR_FALLBACK_TO_STRING},
    {"silently-fallback-to-string", ON_ERROR_FALLBACK_TO_STRING | ON_ERROR_SILENT}
  };

  return cr_make_param_array(TemplateTestCase, test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
}

ParameterizedTest(TemplateTestCase *test_cases, template_on_err, test_success)
{
  gint r;

  cr_assert(log_template_on_error_parse(test_cases->actual, &r), "Parsing '%s' works", test_cases->actual);
  cr_assert_eq(r, test_cases->expected, "'%s' parses down to '%d'", test_cases->actual, test_cases->expected);
}

Test(template_on_error, test_fail)
{
  gint r;
  gchar *pattern = "do-what-i-mean";

  cr_assert_not(log_template_on_error_parse(pattern, &r), "Parsing '%s' works", pattern);
}

TestSuite(template_on_error, .init = app_startup, .fini = app_shutdown);
