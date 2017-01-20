/*
 * Copyright (c) 2016-2017 Noémi Ványi
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

#include "cfg.h"
#include "cli.h"

#include <criterion/criterion.h>


Test(cli, test_cli_param_converter_setup)
{
  gchar **parser_raw_param;

  parser_raw_param = (char *[])
  { "parser csvparser()", NULL
  };
  CliParamConverter *converter = cli_param_converter_new(parser_raw_param);
  cr_assert_not(cli_param_converter_setup(converter), "Invalid raw params: missing driver separator");

  parser_raw_param = (char *[])
  { "csvparser();", NULL
  };
  converter = cli_param_converter_new(parser_raw_param);
  cr_assert_not(cli_param_converter_setup(converter), "Invalid raw params: missing driver type");

  parser_raw_param = (char *[])
  { "parser;", NULL
  };
  converter = cli_param_converter_new(parser_raw_param);
  cr_assert_not(cli_param_converter_setup(converter), "Invalid raw params: missing driver config");

  parser_raw_param = (char *[])
  { "parser json-parser();", "parser json-parser()", NULL
  };
  converter = cli_param_converter_new(parser_raw_param);
  cr_assert_not(cli_param_converter_setup(converter), "Invalid raw params: missing driver separator in 2nd item");

  parser_raw_param = (char *[])
  { "parser csvparser();", "parser;", NULL
  };
  converter = cli_param_converter_new(parser_raw_param);
  cr_assert_not(cli_param_converter_setup(converter), "Invalid raw params: missing driver config in 2nd item");

  parser_raw_param = (char *[])
  { "parser csvparser();", "csvparser();", NULL
  };
  converter = cli_param_converter_new(parser_raw_param);
  cr_assert_not(cli_param_converter_setup(converter), "Invalid raw params: missing driver config in 2nd item");

  parser_raw_param = (char *[])
  { "parser csvparser();", NULL
  };
  converter = cli_param_converter_new(parser_raw_param);
  cr_assert(cli_param_converter_setup(converter), "Valid raw params: csvparser");
  cr_assert(g_list_length(converter->params) == 1, "1 parsed driver: csvparser");

  parser_raw_param = (char *[])
  { "parser csvparser(columns(column));", NULL
  };
  converter = cli_param_converter_new(parser_raw_param);
  cr_assert(cli_param_converter_setup(converter), "Valid raw params: csvparser");
  cr_assert(g_list_length(converter->params) == 1, "1 parsed driver: csvparser");

  parser_raw_param = (char *[])
  { "parser csvparser(columns(column)); parser json-parser();", NULL
  };
  converter = cli_param_converter_new(parser_raw_param);
  cr_assert(cli_param_converter_setup(converter), "Valid raw params: csvparser, json-parser");
  cr_assert(g_list_length(converter->params) == 2, "2 parsed driver: csvparser, json-parser");
}

Test(cli, test_cli_initialize_config_with_one_param)
{
  GList *params = NULL;
  CliParam *cli_param = cli_param_new("parser", "csvparser();");
  cli_param->name = "my-name";
  params = g_list_append(params, cli_param);
  CliParamConverter *converter = cli_param_converter_new(NULL);
  converter->params = params;

  gchar expected_cfg[200];
  gchar *expected_cfg_template = "@version: %s\n"
                                 "@include scl.conf\n"
                                 "source s_stdin { stdin(); };\n"
                                 "destination d_stdout { stdout(); };\n"
                                 " parser my-name { csvparser();};\n"
                                 "log {source(s_stdin); parser(my-name); destination(d_stdout);};";
  g_snprintf(expected_cfg, sizeof(expected_cfg), expected_cfg_template, SYSLOG_NG_VERSION);

  cli_param_converter_convert(converter);
  cr_assert_eq(strcmp(converter->generated_config, expected_cfg), 0, "Generated: \n%s\nExpected:\n%s",
               converter->generated_config,
               expected_cfg);
}

Test(cli, test_cli_initialize_config_with_two_params_param)
{
  GList *params = NULL;
  CliParam *cli_param = cli_param_new("parser", "csvparser();");
  cli_param->name = "my-csvparser-name";
  params = g_list_append(params, cli_param);
  cli_param = cli_param_new("parser", "jsonparser();");
  cli_param->name = "my-jsonparser-name";
  params = g_list_append(params, cli_param);
  CliParamConverter *converter = cli_param_converter_new(NULL);
  converter->params = params;

  gchar expected_cfg[300];
  gchar *expected_cfg_template = "@version: %s\n"
                                 "@include scl.conf\n"
                                 "source s_stdin { stdin(); };\n"
                                 "destination d_stdout { stdout(); };\n"
                                 " parser my-csvparser-name { csvparser();};\n"
                                 " parser my-jsonparser-name { jsonparser();};\n"
                                 "log {source(s_stdin); parser(my-csvparser-name); parser(my-jsonparser-name); destination(d_stdout);};";
  g_snprintf(expected_cfg, sizeof(expected_cfg), expected_cfg_template, SYSLOG_NG_VERSION);

  cli_param_converter_convert(converter);
  cr_assert_eq(strcmp(converter->generated_config, expected_cfg), 0, "Generated: \n%s\nExpected:\n%s",
               converter->generated_config, expected_cfg);
}
