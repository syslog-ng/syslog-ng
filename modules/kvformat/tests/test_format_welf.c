/*
 * Copyright (c) 2011-2018 Balabit
 * Copyright (c) 2011-2013 Gergely Nagy <algernon@balabit.hu>
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
#include <criterion/parameterized.h>

#include "libtest/cr_template.h"
#include "apphook.h"
#include "plugin.h"
#include "cfg.h"
#include "logmsg/logmsg.h"

void
setup(void)
{
  app_startup();
  setenv("TZ", "UTC", TRUE);
  tzset();
  init_template_tests();
  cfg_load_module(configuration, "kvformat");
}

void
teardown(void)
{
  deinit_template_tests();
  app_shutdown();
}

TestSuite(format_welf, .init = setup, .fini = teardown);

Test(format_welf, test_format_welf)
{
  assert_template_format("$(format-welf MSG=$MSG)", "MSG=árvíztűrőtükörfúrógép");
  assert_template_format("xxx$(format-welf MSG=$MSG)yyy", "xxxMSG=árvíztűrőtükörfúrógépyyy");
  assert_template_format("$(format-welf MSG=$escaping)",
                         "MSG=\"binary stuff follows \\\"\\xad árvíztűrőtükörfúrógép\"");
  assert_template_format("$(format-welf MSG=$escaping2)", "MSG=\\xc3");
  assert_template_format("$(format-welf MSG=$null)", "MSG=binary\\x00stuff");
  assert_template_format_with_context("$(format-welf MSG=$MSG)",
                                      "MSG=árvíztűrőtükörfúrógép MSG=árvíztűrőtükörfúrógép");
}

Test(format_welf, test_format_welf_performance)
{
  perftest_template("$(format-welf APP.*)\n");
  perftest_template("<$PRI>1 $ISODATE $LOGHOST @syslog-ng - - ${SDATA:--} $(format-welf --scope all-nv-pairs "
                    "--exclude 0* --exclude 1* --exclude 2* --exclude 3* --exclude 4* --exclude 5* "
                    "--exclude 6* --exclude 7* --exclude 8* --exclude 9* "
                    "--exclude SOURCE "
                    "--exclude .SDATA.* "
                    "..RSTAMP='${R_UNIXTIME}${R_TZ}' "
                    "..TAGS=${TAGS})\n");
}

typedef struct
{
  gchar *name;
  gchar *value;
} NVPair;

static LogMessage *
create_message_with_nvpairs(NVPair *pairs)
{
  LogMessage *msg = log_msg_new_empty();

  for (int i = 0; pairs[i].name; i++)
    {
      gchar *name = pairs[i].name;
      gchar *value = pairs[i].value;
      log_msg_set_value_by_name(msg, name, value, -1);
    }

  return msg;
}

static void
assert_template(LogMessage *msg, const gchar *template_str, const gchar *expected)
{
  LogTemplate *template =  log_template_new(configuration, NULL);
  cr_assert(log_template_compile(template, template_str, NULL));
  GString *result = g_string_new("");

  log_template_format(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, result);
  cr_assert_str_eq(result->str, expected, "Template (%s) result different: %s != %s",
                   template_str, result->str, expected);

  g_string_free(result, TRUE);
  log_template_unref(template);
}

Test(format_welf, test_space)
{

  NVPair pairs[] =
  {
    {"with_space", "with space"},
    {NULL, NULL}
  };


  LogMessage *msg = create_message_with_nvpairs(pairs);
  assert_template(msg, "$(format-welf --key with_space)",
                  "with_space=\"with space\"");
  log_msg_unref(msg);
}

struct test_params
{
  gchar *template;
  gchar *expected;
};

ParameterizedTestParameters(format_welf, key_and_exclude)
{
  static struct test_params params[] =
  {
    {"$(format-welf --key prefix.key*)", "prefix.key1=value1 prefix.key2=value2"},
    {"$(format-welf --key prefix.key1,prefix.key2)", "prefix.key1=value1 prefix.key2=value2"},
    {"$(format-welf --key prefix.* --exclude prefix.exclude*)", "prefix.key1=value1 prefix.key2=value2"},
    {"$(format-welf --key prefix.* --exclude prefix.exclude1,prefix.exclude2)", "prefix.key1=value1 prefix.key2=value2"},
    {"$(format-welf --key prefix.* --exclude prefix.exclude1 --exclude prefix.exclude2)", "prefix.key1=value1 prefix.key2=value2"},
    {"$(format-welf --key prefix.* --exclude prefix.exclude* --exclude prefix.key2)", "prefix.key1=value1"},
  };

  return cr_make_param_array(struct test_params, params, sizeof(params)/sizeof(params[0]));
}

ParameterizedTest(struct test_params *param, format_welf, key_and_exclude)
{
  static NVPair pairs[] =
  {
    {"prefix.key1", "value1"},
    {"prefix.key2", "value2"},
    {"prefix.exclude1", "exclude1"},
    {"prefix.exclude2", "exclude2"},
    {NULL, NULL}
  };

  LogMessage *msg = create_message_with_nvpairs(pairs);
  assert_template(msg, param->template, param->expected);
  log_msg_unref(msg);
}
