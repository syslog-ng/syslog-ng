/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 Kokan
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
#include "apphook.h"

#include <criterion/criterion.h>


Test(test_apphook, app_is_tarting_up)
{
  cr_assert(app_is_starting_up());
  cr_assert_not(app_is_shutting_down());
}

static void
_hook_counter(gint type, gpointer user_data)
{
  gint *triggered_count = (gboolean *)user_data;
  (*triggered_count)++;
}

Test(test_apphook, post_daemon_hook)
{
  gint triggered_count = 0;
  register_application_hook(AH_POST_DAEMONIZED, _hook_counter, (gpointer)&triggered_count);
  cr_assert(app_is_starting_up());
  app_post_daemonized();

  cr_assert_eq(triggered_count, 1);
}

Test(test_apphook, post_daemon_triggered_twice)
{
  gint triggered_count = 0;
  register_application_hook(AH_POST_DAEMONIZED, _hook_counter, (gpointer)&triggered_count);

  app_post_daemonized();
  app_post_daemonized();

  cr_assert_eq(triggered_count, 1);
}

Test(test_apphook, pre_shutdown_hook)
{
  gint triggered_count = 0;
  register_application_hook(AH_PRE_SHUTDOWN, _hook_counter, (gpointer)&triggered_count);

  app_pre_shutdown();

  cr_assert_eq(triggered_count, 1);
}

Test(test_apphook, shutdown_hook)
{
  gint triggered_count = 0;
  register_application_hook(AH_SHUTDOWN, _hook_counter, (gpointer)&triggered_count);

  app_startup(); //This is needed for shutdown
  app_shutdown();

  cr_assert_eq(triggered_count, 1);
}

Test(test_apphook, config_changed)
{
  gint triggered_count = 0;
  register_application_hook(AH_CONFIG_CHANGED, _hook_counter, (gpointer)&triggered_count);

  app_config_changed();

  cr_assert_eq(triggered_count, 1);
}


Test(test_apphook, reopen_hook)
{
  gint triggered_count = 0;
  register_application_hook(AH_REOPEN_FILES, _hook_counter, (gpointer)&triggered_count);

  app_reopen_files();

  cr_assert_eq(triggered_count, 1);
}

static void
_re_registering_hook(gint type, gpointer user_data)
{
  _hook_counter(type, user_data);
  register_application_hook(type, _re_registering_hook, user_data);
}

Test(test_apphook, hook_register_from_hook)
{
  gint triggered_count = 0;

  register_application_hook(AH_REOPEN_FILES, _re_registering_hook, (gpointer)&triggered_count);

  app_reopen_files();
  cr_assert_eq(triggered_count, 1);
}

Test(test_apphook, hook_register_from_hook_and_other_not_triggered_hooks)
{
  gint triggered_count = 0;

  register_application_hook(AH_PRE_SHUTDOWN, NULL, NULL);
  register_application_hook(AH_REOPEN_FILES, _re_registering_hook, (gpointer)&triggered_count);
  register_application_hook(AH_PRE_SHUTDOWN, NULL, NULL);

  app_reopen_files();
  cr_assert_eq(triggered_count, 1);
}

Test(test_apphook, trigger_all_state_hook)
{
  gint triggered_count = 0;
  register_application_hook(AH_POST_DAEMONIZED, _hook_counter, (gpointer)&triggered_count);
  register_application_hook(AH_RUNNING, _hook_counter, (gpointer)&triggered_count);
  register_application_hook(AH_PRE_SHUTDOWN, _hook_counter, (gpointer)&triggered_count);
  register_application_hook(AH_SHUTDOWN, _hook_counter, (gpointer)&triggered_count);

  register_application_hook(AH_CONFIG_CHANGED, _hook_counter, (gpointer)&triggered_count);
  register_application_hook(AH_REOPEN_FILES, _hook_counter, (gpointer)&triggered_count);

  app_startup();
  app_post_daemonized();
  cr_assert_eq(triggered_count, 1);

  app_running();
  cr_assert_eq(triggered_count, 2);

  /* check that a state that has already passed would be invoked immediately */
  register_application_hook(AH_POST_DAEMONIZED, _hook_counter, (gpointer)&triggered_count);

  cr_assert_eq(triggered_count, 3);

  app_config_changed();
  app_reopen_files();
  cr_assert_eq(triggered_count, 5);

  app_pre_shutdown();
  app_shutdown();

  cr_assert_eq(triggered_count, 7);
}

