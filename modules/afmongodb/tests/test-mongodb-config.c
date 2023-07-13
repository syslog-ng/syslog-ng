/*
 * Copyright (c) 2016 Balabit
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
#include "libtest/cr_template.h"
#include "libtest/grab-logging.h"

#include "apphook.h"
#include "logthrdest/logthrdestdrv.h"
#include "../afmongodb-parser.h"

#define SAFEOPTS "?wtimeoutMS=60000&socketTimeoutMS=60000&connectTimeoutMS=60000"

static LogDriver *mongodb;

static void
assert_uri_present_in_log(const gchar *uri, const gchar *db, const gchar *coll)
{
  GString *pattern = g_string_sized_new(0);
  g_string_append_printf(pattern,
                         "Initializing MongoDB destination;"
                         " uri='mongodb://%s', db='%s', collection='%s'",
                         uri, db, coll);
  assert_grabbed_log_contains(pattern->str);
  g_string_free(pattern, TRUE);
}

Test(mongodb_config, test_persist_name)
{
  afmongodb_dd_set_uri(mongodb, "mongodb://127.0.0.2:27018,localhost:1234/syslog"
                       SAFEOPTS "&replicaSet=x");

  cr_assert(afmongodb_dd_private_uri_init(mongodb));
  cr_assert_str_eq(log_pipe_get_persist_name(&mongodb->super), "afmongodb(127.0.0.2:27018,syslog,x,messages)");
}

Test(mongodb_config, test_stats_name)
{
  afmongodb_dd_set_uri(mongodb, "mongodb://127.0.0.2:27018,localhost:1234/syslog"
                       SAFEOPTS "&replicaSet=x");

  cr_assert(afmongodb_dd_private_uri_init(mongodb));

  LogThreadedDestDriver *self = (LogThreadedDestDriver *)mongodb;
  const gchar *name = self->format_stats_key(self, NULL);
  cr_assert(name, "mongodb,127.0.0.2:27018,syslog,x,messages");
}

Test(mongodb_config, test_uri_components_are_parsed_and_reported_correctly)
{
  reset_grabbed_messages();
  cr_assert(afmongodb_dd_private_uri_init(mongodb));
  assert_uri_present_in_log("127.0.0.1:27017/syslog" SAFEOPTS, "syslog", "messages");
  reset_grabbed_messages();

  afmongodb_dd_set_uri(mongodb, "mongodb://%2Ftmp%2Fmongo.sock/syslog");
  cr_assert(afmongodb_dd_private_uri_init(mongodb));
  assert_uri_present_in_log("%2Ftmp%2Fmongo.sock/syslog", "syslog", "messages");
  reset_grabbed_messages();

  afmongodb_dd_set_uri(mongodb, "mongodb://localhost:1234/syslog-ng");
  cr_assert(afmongodb_dd_private_uri_init(mongodb));
  assert_uri_present_in_log("localhost:1234/syslog-ng", "syslog-ng", "messages");
  reset_grabbed_messages();
}

Test(mongodb_config, test_collection_option_is_consumed_and_reported_correctly)
{
  afmongodb_dd_set_collection(mongodb, compile_template("messages2"));
  cr_assert(afmongodb_dd_private_uri_init(mongodb));
  assert_uri_present_in_log("127.0.0.1:27017/syslog" SAFEOPTS, "syslog", "messages2");
}

Test(mongodb_config, test_invalid_uris_are_reported_as_errors)
{
  afmongodb_dd_set_uri(mongodb, "INVALID-URI");
  cr_assert_not(afmongodb_dd_private_uri_init(mongodb));
  assert_grabbed_log_contains("Error parsing MongoDB URI; uri='INVALID-URI'");
  reset_grabbed_messages();

  afmongodb_dd_set_uri(mongodb, "mongodb://127.0.0.1:27017/");
  cr_assert_not(afmongodb_dd_private_uri_init(mongodb));
  assert_grabbed_log_contains("Missing DB name from MongoDB URI; uri='mongodb://127.0.0.1:27017/'");
}

static void
setup(void)
{
  /* we are relying on verbose messages in tests */
  verbose_flag = TRUE;
  app_startup();

  configuration = cfg_new_snippet();

  const gchar *persist_filename = "";
  configuration->state = persist_state_new(persist_filename);

  start_grabbing_messages();
  mongodb = afmongodb_dd_new(configuration);
}

static void
teardown(void)
{

  reset_grabbed_messages();
  if (mongodb)
    {
      log_pipe_unref(&mongodb->super);
      mongodb = NULL;
    }

  if (configuration->persist)
    {
      persist_config_free(configuration->persist);
      configuration->persist = NULL;
    }
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(mongodb_config,  .init = setup, .fini = teardown);
