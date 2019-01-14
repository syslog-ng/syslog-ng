/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Viktor Tusa
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

#include <value-pairs/value-pairs.h>

#include <criterion/criterion.h>
#include <libtest/testutils.h>

#include <apphook.h>
#include <plugin.h>
#include <cfg.h>
#include "logmsg/logmsg.h"

MsgFormatOptions parse_options;
LogTemplateOptions template_options;
GlobalConfig *cfg;

int root_data = 1;
int root_test_data = 2;

static gboolean
test_vp_obj_start(const gchar *name,
                  const gchar *prefix, gpointer *prefix_data,
                  const gchar *prev, gpointer *prev_data,
                  gpointer user_data)
{
  static int times_called = 0;

  switch(times_called)
    {
    case 0:
      cr_expect_null(prefix, "First vp_obj_start but prefix is not NULL!");
      break;
    case 1:
      cr_expect_str_eq(prefix, "root", "Second vp_obj_start but prefix is not 'root'!");
      *prefix_data = &root_data;
      break;
    case 2:
      cr_expect_str_eq(prefix, "root.test", "Third vp_obj_start but prefix is not 'root.test'!");
      cr_expect_str_eq(prev, "root", "Wrong previous prefix");
      cr_expect_eq(*((gint *)(*prev_data)), root_data, "Wrong previous data");
      *prefix_data = &root_test_data;
      break;
    default:
      cr_expect_fail("vp_obj_start called more times than number of path elements!");
    }
  times_called++;
  return FALSE;
}

static gboolean
test_vp_obj_stop(const gchar *name,
                 const gchar *prefix, gpointer *prefix_data,
                 const gchar *prev, gpointer *prev_data,
                 gpointer user_data)
{
  static int times_called = 0;

  switch(times_called)
    {
    case 0:
      cr_expect_str_eq(prefix, "root.test", "First vp_obj_stop but prefix is not 'root.test'!");
      cr_expect_str_eq(prev, "root", "Wrong previous prefix");
      cr_expect_eq(*((gint *)(*prev_data)), root_data, "Wrong previous data");
      break;
    case 1:
      cr_expect_str_eq(prefix, "root", "Second vp_obj_stop but prefix is not 'root'!");
      break;
    case 2:
      cr_expect_null(prefix, "Third vp_obj_stop but prefix is not NULL!");
      break;
    default:
      cr_expect_fail("vp_obj_stop called more times than number of path elements!");
    }
  times_called++;
  return FALSE;

}

static gboolean
test_vp_value(const gchar *name, const gchar *prefix,
              TypeHint type, const gchar *value, gsize value_len,
              gpointer *prefix_data, gpointer user_data)
{
  cr_expect_str_eq(prefix, "root.test", "Wrong prefix");
  cr_expect_str_eq(value, "value", "Wrong value");
  cr_expect_eq(*((gint *)(*prefix_data)), root_test_data, "Wrong prefix data");

  return FALSE;
}

Test(value_pairs_walker, prefix_dat)
{
  ValuePairs *vp;
  LogMessage *msg;
  const char *value = "value";

  log_template_options_init(&template_options, cfg);
  msg_format_options_init(&parse_options, cfg);

  vp = value_pairs_new();
  value_pairs_add_glob_pattern(vp, "root.*", TRUE);
  msg = log_msg_new("test", 4, &parse_options);

  log_msg_set_value_by_name(msg, "root.test.alma", value, strlen(value));
  log_msg_set_value_by_name(msg, "root.test.korte", value, strlen(value));

  value_pairs_walk(vp, test_vp_obj_start, test_vp_value, test_vp_obj_stop, msg, 0, LTZ_LOCAL, &template_options, NULL);
  value_pairs_unref(vp);
  log_msg_unref(msg);
};


void
setup(void)
{

  app_startup();

  cfg = cfg_new_snippet();
  cfg_load_module(cfg, "syslogformat");
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, cfg);
}

void
teardown(void)
{
  app_shutdown();
}

TestSuite(value_pairs_walker, .init = setup, .fini = teardown);

