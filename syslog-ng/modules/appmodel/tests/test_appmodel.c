/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balazs Scheidler <balazs.scheidler@balabit.com>
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
#include "appmodel.h"
#include "application.h"
#include "plugin.h"
#include "apphook.h"
#include "cfg-grammar.h"
#include "config_parse_lib.h"

Application *
_parse_application(const char *appmodel, const gchar *name, const gchar *topic)
{
  AppModelContext *ac;

  cfg_load_module(configuration, "appmodel");
  cr_assert(parse_config(appmodel, LL_CONTEXT_ROOT, NULL, NULL),
            "Parsing the given configuration failed: %s", appmodel);
  ac = appmodel_get_context(configuration);
  return appmodel_context_lookup_application(ac, name, topic);
}

Test(appmodel, empty_application_can_be_parsed_properly)
{
  Application *app;

  app = _parse_application("application foobar[*] {};", "foobar", "*");
  cr_assert(app != NULL);
  cr_assert_str_eq(app->name, "foobar");
  cr_assert_str_eq(app->topic, "*");
}

Test(appmodel, name_is_parsed_into_name_member)
{
  Application *app;

  app = _parse_application("application name[*] {};", "name", "*");
  cr_assert(app != NULL);
}

Test(appmodel, topic_in_brackets_is_parsed_into_topic)
{
  Application *app;

  app = _parse_application("application name[port514] {};", "name", "port514");
  cr_assert(app != NULL);
  cr_assert_str_eq(app->topic, "port514");
}

Test(appmodel, filter_expressions_can_be_specified_with_a_filter_keyword)
{
  Application *app;

  app = _parse_application(
          "application name[port514] {"
          "  filter { program(\"kernel\"); };"
          "  parser { kv-parser(); };"
          "};",
          "name", "port514");
  cr_assert(app != NULL);
  cr_assert_str_eq(app->topic, "port514");
  cr_assert_str_eq(app->filter_expr, " program(\"kernel\"); ");
}

Test(appmodel, parser_expressions_can_be_specified_with_a_parser_keyword)
{
  Application *app;

  app = _parse_application(
          "application name[port514] {"
          "  parser { kv-parser(); };"
          "  filter { program(\"kernel\"); };"
          "};",
          "name", "port514");
  cr_assert(app != NULL);
  cr_assert_str_eq(app->topic, "port514");
  cr_assert_str_eq(app->parser_expr, " kv-parser(); ");
}

static void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
}

void
teardown(void)
{
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(appmodel, .init = setup, .fini = teardown);
