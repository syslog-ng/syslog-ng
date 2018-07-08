/*
 * Copyright (c) 2010-2018 Balabit
 * Copyright (c) 2010-2014 Balázs Scheidler <balazs.scheidler@balabit.com>
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
#include "apphook.h"
#include "logmsg/logmsg.h"
#include "patterndb.h"
#include "cfg.h"
#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#define MYHOST "MYHOST"

gchar *pdb_parser_skeleton_prefix ="<?xml version='1.0' encoding='UTF-8'?>\
<patterndb version='4' pub_date='2010-02-22'>\
  <ruleset name='test1_program' id='480de478-d4a6-4a7f-bea4-0c0245d361e1'>\
    <patterns>\
      <pattern>test</pattern>\
    </patterns>\
    <rules>\
      <rule id='1' class='test1' provider='my'>\
        <patterns>\
          <pattern>" /* HERE COMES THE GENERATED PATTERN */ ;

gchar *pdb_parser_skeleton_postfix =  /* HERE IS THE GENERATED PATTERN */ "</pattern>\
        </patterns>\
      </rule>\
    </rules>\
  </ruleset>\
</patterndb>";

void
_assert_pattern(PatternDB *patterndb, const gchar *pattern, const gchar *rule, gboolean match)
{
  gboolean result;
  LogMessage *msg = log_msg_new_empty();

  log_msg_set_value(msg, LM_V_HOST, MYHOST, -1);
  log_msg_set_value(msg, LM_V_PROGRAM, "test", -1);
  log_msg_set_value(msg, LM_V_MESSAGE, pattern, -1);

  result = pattern_db_process(patterndb, msg);

  if (match)
    {
      cr_assert(result, "Value '%s' is not matching for pattern '%s'\n", rule, pattern);
    }
  else
    {
      cr_assert_not(result, "Value '%s' is matching for pattern '%s'\n", rule, pattern);
    }

  log_msg_unref(msg);
}

static PatternDB *
_load_pattern_db_from_string(const gchar *pdb, gchar **filename)
{
  PatternDB *patterndb = pattern_db_new();

  g_file_open_tmp("patterndbXXXXXX.xml", filename, NULL);
  g_file_set_contents(*filename, pdb, strlen(pdb), NULL);

  cr_assert(pattern_db_reload_ruleset(patterndb, configuration, *filename), "Error loading ruleset [[[%s]]]", pdb);
  cr_assert_str_eq(pattern_db_get_ruleset_pub_date(patterndb), "2010-02-22", "Invalid pubdate");

  return patterndb;
}

static void
_destroy_pattern_db(PatternDB *patterndb, gchar *filename)
{
  pattern_db_free(patterndb);
  g_unlink(filename);
}

typedef struct _test_parsers_e2e_param
{
  const gchar *rule;
  const gchar *pattern;
  gboolean match;
} TestParsersE2EParam;

ParameterizedTestParameters(parsers_e2e, test_parsers)
{
  static TestParsersE2EParam parser_params[] =
  {
    {"@ANYSTRING:TEST@", "ab ba ab", TRUE},
    {"@ANYSTRING:TEST@", "1234ab", TRUE},
    {"@ANYSTRING:TEST@", "ab1234", TRUE},
    {"@ANYSTRING:TEST@", "1.2.3.4", TRUE},
    {"@ANYSTRING:TEST@", "ab  1234  ba", TRUE},
    {"@ANYSTRING:TEST@", "&lt;ab ba&gt;", TRUE},
    {"@DOUBLE:TEST@", "1234", TRUE},
    {"@DOUBLE:TEST@", "1234.567", TRUE},
    {"@DOUBLE:TEST@", "1.2.3.4", TRUE},
    {"@DOUBLE:TEST@", "1234ab", TRUE},
    {"@DOUBLE:TEST@", "ab1234", FALSE},
    {"@ESTRING:TEST:endmark@","ab ba endmark", TRUE},
    {"@ESTRING:TEST:endmark@","ab ba", FALSE},
    {"@ESTRING:TEST:&gt;@","ab ba > ab", TRUE},
    {"@ESTRING:TEST:&gt;@","ab ba", FALSE},
    {"@FLOAT:TEST@", "1234", TRUE},
    {"@FLOAT:TEST@", "1234.567", TRUE},
    {"@FLOAT:TEST@", "1.2.3.4", TRUE},
    {"@FLOAT:TEST@", "1234ab", TRUE},
    {"@FLOAT:TEST@", "ab1234", FALSE},
    {"@SET:TEST: 	@", " a ", TRUE},
    {"@SET:TEST: 	@", "  a ", TRUE},
    {"@SET:TEST: 	@", " 	a ", TRUE},
    {"@SET:TEST: 	@", " 	 a ", TRUE},
    {"@SET:TEST: 	@", "ab1234", FALSE},
    {"@IPv4:TEST@", "1.2.3.4", TRUE},
    {"@IPv4:TEST@", "0.0.0.0", TRUE},
    {"@IPv4:TEST@", "255.255.255.255", TRUE},
    {"@IPv4:TEST@", "256.256.256.256", FALSE},
    {"@IPv4:TEST@", "1234", FALSE},
    {"@IPv4:TEST@", "ab1234", FALSE},
    {"@IPv4:TEST@", "ab1.2.3.4", FALSE},
    {"@IPv4:TEST@", "1,2,3,4", FALSE},
    {"@IPv6:TEST@", "2001:0db8:0000:0000:0000:0000:1428:57ab", TRUE},
    {"@IPv6:TEST@", "2001:0db8:0000:0000:0000::1428:57ab", TRUE},
    {"@IPv6:TEST@", "2001:0db8:0:0:0:0:1428:57ab", TRUE},
    {"@IPv6:TEST@", "2001:0db8:0:0::1428:57ab", TRUE},
    {"@IPv6:TEST@", "2001:0db8::1428:57ab", TRUE},
    {"@IPv6:TEST@", "2001:db8::1428:57ab", TRUE},
    {"@IPv6:TEST@", "2001:0db8::34d2::1428:57ab", FALSE},
    {"@NUMBER:TEST@", "1234", TRUE},
    {"@NUMBER:TEST@", "1.2", TRUE},
    {"@NUMBER:TEST@", "1.2.3.4", TRUE},
    {"@NUMBER:TEST@", "1234ab", TRUE},
    {"@NUMBER:TEST@", "ab1234", FALSE},
    {"@QSTRING:TEST:&lt;&gt;@", "<aa bb>", TRUE},
    {"@QSTRING:TEST:&lt;&gt;@", "< aabb >", TRUE},
    {"@QSTRING:TEST:&lt;&gt;@", "aabb>", FALSE},
    {"@QSTRING:TEST:&lt;&gt;@", "<aabb", FALSE},
    {"@STRING:TEST@", "aabb", TRUE},
    {"@STRING:TEST@", "aa bb", TRUE},
    {"@STRING:TEST@", "1234", TRUE},
    {"@STRING:TEST@", "ab1234", TRUE},
    {"@STRING:TEST@", "1234bb", TRUE},
    {"@STRING:TEST@", "1.2.3.4", TRUE}
  };

  return cr_make_param_array(TestParsersE2EParam, parser_params, G_N_ELEMENTS(parser_params));
};

ParameterizedTest(TestParsersE2EParam *param, parsers_e2e, test_parsers)
{
  GString *str = g_string_new(pdb_parser_skeleton_prefix);
  g_string_append(str, param->rule);
  g_string_append(str, pdb_parser_skeleton_postfix);

  gchar *filename;
  PatternDB *patterndb = _load_pattern_db_from_string(str->str, &filename);
  g_string_free(str, TRUE);
  _assert_pattern(patterndb, param->pattern, param->rule, param->match);

  _destroy_pattern_db(patterndb, filename);
  g_free(filename);
}

void setup(void)
{
  app_startup();
  msg_init(TRUE);
  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "basicfuncs");
  cfg_load_module(configuration, "syslogformat");
  pattern_db_global_init();
}

void teardown(void)
{
  app_shutdown();
}

TestSuite(parsers_e2e, .init = setup, .fini = teardown);
