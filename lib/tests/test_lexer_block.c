/*
 * Copyright (c) 2018 Balabit
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

#include "syslog-ng.h"
#include "cfg-lexer.h"
#include "cfg-args.h"
#include "cfg-block.h"
#include "cfg-lexer.h"
#include "cfg.h"

#include "criterion/criterion.h"

YYLTYPE yyloc;
GString *result;

static void
setup(void)
{
  result = g_string_sized_new(100);

  CfgIncludeLevel *level = g_new0(CfgIncludeLevel, 1);
  level->name = "";
  yyloc.level = level;

  configuration = cfg_new_snippet();
  msg_init(TRUE);
}

static void
teardown(void)
{
  msg_deinit();
  cfg_free(configuration);
  g_free(yyloc.level);
  g_string_free(result, TRUE);
}

TestSuite(test_block, .init = setup, .fini = teardown);

Test(test_block, mandatory_arguments)
{
  CfgArgs *arg_defs = cfg_args_new();
  cfg_args_set(arg_defs, "mandatory-param", NULL);

  CfgBlockGenerator *generator = cfg_block_new(cfg_lexer_lookup_context_type_by_name("block"),
                                               "block_with_mandatory_options",
                                               "`mandatory_param`",
                                               arg_defs, &yyloc);

  CfgArgs *args = cfg_args_new();
  cr_assert_not(generator->generate(generator, configuration, args, result, ""), "mandatory parameter missing");

  cfg_args_set(args, "mandatory-param", "value");
  cr_assert(generator->generate(generator, configuration, args, result, ""));
  cr_assert_str_eq(result->str, "value");
}

Test(test_block, varargs)
{
  CfgArgs *arg_defs = cfg_args_new();
  CfgBlockGenerator *generator = cfg_block_new(cfg_lexer_lookup_context_type_by_name("block"),
                                               "block_to_test_varargs",
                                               "`varargs_param`",
                                               arg_defs, &yyloc);

  CfgArgs *args = cfg_args_new();
  cfg_args_set(args, "varargs_param", "value");
  cr_assert_not(generator->generate(generator, configuration, args, result, ""), "varargs not set yet");

  cfg_args_accept_varargs(arg_defs);
  cr_assert(generator->generate(generator, configuration, args, result, ""));
  cr_assert_str_eq(result->str, "value");
}
