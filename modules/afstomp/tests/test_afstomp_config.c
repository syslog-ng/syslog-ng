/*
 * Copyright (c) 2026 OSS Keepers
 * Copyright (c) 2026 Vyacheslav Yurkov <uvv.mail@gmail.com>
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

#include "apphook.h"
#include "cfg.h"
#include "cfg-grammar.h"
#include "libtest/config_parse_lib.h"
#include "logthrdest/logthrdestdrv.h"

static LogThreadedDestDriver *
_parse_stomp(const gchar *options)
{
  gchar *config = g_strdup_printf("stomp(host(\"localhost\") %s)", options);
  LogThreadedDestDriver *driver = NULL;

  gboolean success = parse_config(config, LL_CONTEXT_DESTINATION, NULL, (gpointer *) &driver);
  g_free(config);
  return success ? driver : NULL;
}

Test(afstomp_config, time_reopen_max_is_disabled_by_default)
{
  LogThreadedDestDriver *driver = _parse_stomp("");
  cr_assert_not_null(driver);
  cr_assert_eq(driver->time_reopen_max, 0);
  log_pipe_unref(&driver->super.super.super);
}

Test(afstomp_config, time_reopen_max_is_accepted_as_destination_option)
{
  LogThreadedDestDriver *driver = _parse_stomp("time-reopen(5) time-reopen-max(600)");
  cr_assert_not_null(driver);
  cr_assert_eq(driver->time_reopen, 5);
  cr_assert_eq(driver->time_reopen_max, 600);
  log_pipe_unref(&driver->super.super.super);
}

Test(afstomp_config, time_reopen_max_accepts_zero)
{
  LogThreadedDestDriver *driver = _parse_stomp("time-reopen-max(0)");
  cr_assert_not_null(driver);
  cr_assert_eq(driver->time_reopen_max, 0);
  log_pipe_unref(&driver->super.super.super);
}

Test(afstomp_config, time_reopen_max_rejects_negative_values)
{
  cr_assert_null(_parse_stomp("time-reopen-max(-1)"));
}

static void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
  cr_assert(cfg_load_module(configuration, "afstomp"));
}

static void
teardown(void)
{
  cfg_free(configuration);
  configuration = NULL;
  app_shutdown();
}

TestSuite(afstomp_config, .init = setup, .fini = teardown);

