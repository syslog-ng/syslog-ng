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
#include "app-parser-generator.h"
#include "apphook.h"
#include "plugin-types.h"

#include "config_parse_lib.h"

static CfgBlockGenerator *app_parser;
static GString *result;

void
_register_application(const char *appmodel)
{
  cr_assert(parse_config(appmodel, LL_CONTEXT_ROOT, NULL, NULL),
            "Parsing the given configuration failed: %s", appmodel);
}

static CfgBlockGenerator *
_construct_app_parser(void)
{
  return app_parser_generator_new(LL_CONTEXT_PARSER, "app-parser");
}

static void
_app_parser_generate(const gchar *topic)
{
  CfgArgs *args = cfg_args_new();

  cfg_args_set(args, "topic", topic);
  cfg_block_generator_generate(app_parser, configuration, args, result);
  cfg_args_unref(args);
}

static void
_assert_config_is_valid(const gchar *topic)
{
  const gchar *config_fmt = ""
                            "parser p_test {\n"
                            "    app-parser(topic(\"%s\"));\n"
                            "};\n";
  gchar *config;

  config = g_strdup_printf(config_fmt, topic);
  cr_assert(parse_config(config, LL_CONTEXT_ROOT, NULL, NULL),
            "Parsing the given configuration failed: %s", config);
  g_free(config);
}

static void
_assert_snippet_is_present(const gchar *snippet)
{
  cr_assert(strstr(result->str, snippet),
            "Can't find config snippet in generated output: %s, config: >>>%s<<<",
            snippet, result->str);
}

static void
_assert_snippet_is_not_present(const gchar *snippet)
{
  cr_assert(strstr(result->str, snippet) == NULL,
            "Could find config snippet which shouldn't be there in generated output: %s, config: >>>%s<<<",
            snippet, result->str);
}

static void
_assert_application_is_present(const gchar *application)
{
  const gchar *settag_fmt = "set-tag('.app.%s');";
  gchar *settag_snippet = g_strdup_printf(settag_fmt, application);

  cr_assert(strstr(result->str, settag_snippet),
            "Can't find set-tag() invocation for this application: %s, snippet: %s, config: >>>%s<<<",
            application, settag_snippet, result->str);
  g_free(settag_snippet);
}

static void
_assert_parser_framing_is_present(void)
{
  cr_assert(g_str_has_prefix(result->str, "\nchannel"), "Cannot find app-parser() framing (prefix) >>>%s<<<",
            result->str);
  cr_assert(g_str_has_suffix(result->str, "};\n}"), "Cannot find app-parser() framing (suffix): >>>%s<<<", result->str);
}

static void
startup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "appmodel");
  app_parser = _construct_app_parser();
  result = g_string_new("");
}

static void
teardown(void)
{
  g_string_free(result, TRUE);
  app_shutdown();
}

Test(app_parser_generator, app_parser_with_no_apps_registered_generates_empty_framing)
{
  _app_parser_generate("port514");
  _assert_parser_framing_is_present();
  _assert_config_is_valid("port514");
}

Test(app_parser_generator, app_parser_generates_references_to_apps)
{
  _register_application("application foo[port514] {\n"
                        "    filter { program('foo'); };\n"
                        "    parser { kv-parser(prefix('foo.')); };\n"
                        "};");

  _register_application("application bar[port514] {\n"
                        "    filter { program('bar'); };\n"
                        "    parser { kv-parser(prefix('bar.')); };\n"
                        "};");
  _app_parser_generate("port514");
  _assert_parser_framing_is_present();
  _assert_application_is_present("foo");
  _assert_application_is_present("foo");
  _assert_config_is_valid("port514");
}

Test(app_parser_generator, app_parser_uses_filter_or_parser_from_base_topics)
{
  _register_application("application foo[port514] {\n"
                        "};");

  _register_application("application foo[*] {\n"
                        "    filter { program('foo'); };\n"
                        "    parser { kv-parser(prefix('foo.')); };\n"
                        "};");

  _app_parser_generate("port514");
  _assert_parser_framing_is_present();
  _assert_application_is_present("foo");
  _assert_snippet_is_present("program('foo')");
  _assert_snippet_is_present("kv-parser(prefix('foo.'))");
  _assert_config_is_valid("port514");
}

Test(app_parser_generator, app_parser_base_topics_are_skipped)
{
  _register_application("application foo[*] {\n"
                        "    filter { program('foo'); };\n"
                        "    parser { kv-parser(prefix('foo.')); };\n"
                        "};");

  _register_application("application bar[*] {\n"
                        "    filter { program('bar'); };\n"
                        "    parser { kv-parser(prefix('bar.')); };\n"
                        "};");

  _app_parser_generate("port514");
  _assert_parser_framing_is_present();
  _assert_snippet_is_not_present("program('foo')");
  _assert_snippet_is_not_present("program('bar')");
  _assert_config_is_valid("port514");
}

TestSuite(app_parser_generator, .init = startup, .fini = teardown);
