/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#include "control-main.h"
#include "control-server.h"
#include "control-commands.h"
#include <iv.h>
#include <iv_event.h>

struct _ControlServerLoop
{
  GThread *thread;
  const gchar *control_name;
  struct iv_event stop_requested;
};

static void
_server_loop_stop(void *user_data)
{
  ControlServerLoop *self = (ControlServerLoop *) user_data;
  iv_event_unregister(&self->stop_requested);
  iv_quit();
}

static void
_register_stop_requested_event(ControlServerLoop *self)
{
  IV_EVENT_INIT(&self->stop_requested);
  self->stop_requested.handler = _server_loop_stop;
  self->stop_requested.cookie = self;
  iv_event_register(&self->stop_requested);
}

static void
_thread_func(gpointer user_data)
{
  ControlServerLoop *server_loop = (ControlServerLoop *) user_data;
  ControlServer *control_server = control_server_new(server_loop->control_name);

  iv_init();
  {
    _register_stop_requested_event(server_loop);
    control_server_start(control_server);
    iv_main();
    control_server_free(control_server);
  }
  iv_deinit();
}

ControlServerLoop *
control_init(const gchar *control_name)
{
  ControlServerLoop *self = g_new0(ControlServerLoop, 1);
  self->control_name = control_name;
  self->thread = g_thread_new(control_name, (GThreadFunc) _thread_func, self);

  return self;
}

void
control_deinit(ControlServerLoop *self)
{
  if (!self)
    return;

  iv_event_post(&self->stop_requested);
  g_thread_join(self->thread);

  reset_control_command_list();
  g_free(self);
}
