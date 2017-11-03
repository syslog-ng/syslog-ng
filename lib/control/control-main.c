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

typedef struct _ControlServerLoop
{
  GThread *thread;
  MainLoop *main_loop;
  const gchar *control_name;
  struct iv_event stop_requested;
} ControlServerLoop;

static ControlServerLoop control_server_loop;

static void
_thread_stop()
{
  iv_quit();
}

static void
_register_stop_requested_event(ControlServerLoop *self)
{
  IV_EVENT_INIT(&self->stop_requested);
  self->stop_requested.handler = _thread_stop;
  iv_event_register(&self->stop_requested);
}

static void
_thread(gpointer user_data)
{
  ControlServerLoop *cs_thread = (ControlServerLoop *)user_data;
  ControlServer *control_server = control_server_new(cs_thread->control_name,
                                                     control_register_default_commands(cs_thread->main_loop));
  iv_init();
  {
    _register_stop_requested_event(cs_thread);
    control_server_start(control_server);
    iv_main();
    control_server_free(control_server);
  }
  iv_deinit();
}

void
control_thread_start(MainLoop *main_loop, const gchar *control_name)
{
  control_server_loop.main_loop = main_loop;
  control_server_loop.control_name = control_name;
  control_server_loop.thread = g_thread_create((GThreadFunc) _thread, &control_server_loop, TRUE, NULL);
}

void
control_thread_stop(void)
{
  iv_event_post(&control_server_loop.stop_requested);
  g_thread_join(control_server_loop.thread);
}

