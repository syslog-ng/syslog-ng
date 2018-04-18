/*
 * Copyright (c) 2002-2018 Balabit
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

#include "hook-commands.h"
#include "apphook.h"

#include <stdlib.h>

#define HOOK_COMMANDS_PLUGIN "hook-commands"

void
hook_commands_plugin_set_startup(HookCommandsPlugin *self, const gchar *startup)
{
  g_free(self->startup);
  self->startup = g_strdup(startup);
}

void
hook_commands_plugin_set_setup(HookCommandsPlugin *self, const gchar *setup)
{
  g_free(self->setup);
  self->setup = g_strdup(setup);
}

void
hook_commands_plugin_set_teardown(HookCommandsPlugin *self, const gchar *teardown)
{
  g_free(self->teardown);
  self->teardown = g_strdup(teardown);
}

void
hook_commands_plugin_set_shutdown(HookCommandsPlugin *self, const gchar *shutdown)
{
  g_free(self->shutdown);
  self->shutdown = g_strdup(shutdown);
}

static gboolean
_run_hook(LogDriver *driver, const gchar *hook, const gchar *cmd)
{
  gint rc;

  if (!cmd)
    return TRUE;

  msg_debug("About to execute a hook command",
            evt_tag_str("driver", driver->id),
            log_pipe_location_tag(&driver->super),
            evt_tag_str("hook", hook),
            evt_tag_str("command", cmd));
  rc = system(cmd);
  if (rc != 0)
    {
      msg_error("Hook command returned with failure, aborting initialization",
                evt_tag_str("driver", driver->id),
                log_pipe_location_tag(&driver->super),
                evt_tag_str("hook", hook),
                evt_tag_str("command", cmd),
                evt_tag_int("rc", rc));
    }
  return rc == 0;
}

/* this is running in the context of the LogDriver, e.g.  LogPipe *s points
 * to the driver we were plugged into */
static gboolean
_init_hook(LogPipe *s)
{
  LogDriver *driver = (LogDriver *) s;
  HookCommandsPlugin *self = log_driver_get_plugin(driver, HookCommandsPlugin, HOOK_COMMANDS_PLUGIN);

  if (app_is_starting_up() && !_run_hook(driver, "startup", self->startup))
    return FALSE;

  if (!_run_hook(driver, "setup", self->setup))
    return FALSE;
  return self->saved_init(s);
}

static gboolean
_deinit_hook(LogPipe *s)
{
  LogDriver *driver = (LogDriver *) s;
  HookCommandsPlugin *self = log_driver_get_plugin(driver, HookCommandsPlugin, HOOK_COMMANDS_PLUGIN);

  _run_hook(driver, "teardown", self->teardown);

  if (app_is_shutting_down())
    _run_hook(driver, "shutdown", self->shutdown);

  /* NOTE: we call deinit() even if teardown failed, there's not much we can
   * do apart from reporting the failure, which we already did in
   * _run_hook()
   * */

  return self->saved_deinit(s);
}

static gboolean
_attach(LogDriverPlugin *s, LogDriver *driver)
{
  HookCommandsPlugin *self = (HookCommandsPlugin *) s;

  self->saved_init = driver->super.init;
  driver->super.init = _init_hook;

  self->saved_deinit = driver->super.deinit;
  driver->super.deinit = _deinit_hook;
  return TRUE;
}

static void
_free(LogDriverPlugin *s)
{
  HookCommandsPlugin *self = (HookCommandsPlugin *) s;

  g_free(self->setup);
  g_free(self->teardown);
  log_driver_plugin_free_method(s);
}

HookCommandsPlugin *
hook_commands_plugin_new(void)
{
  HookCommandsPlugin *self = g_new0(HookCommandsPlugin, 1);

  log_driver_plugin_init_instance(&self->super, HOOK_COMMANDS_PLUGIN);
  self->super.attach = _attach;
  self->super.free_fn = _free;
  return self;
}
