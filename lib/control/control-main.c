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
control_server_loop_init_instance(ControlServerLoop *self, MainLoop *main_loop, const gchar *control_name)
{
  self->control_name = g_strdup(control_name);
  self->main_loop = main_loop_ref(main_loop);
}

void
control_server_loop_deinit_instance(ControlServerLoop *self)
{
  g_free(self->control_name);
  main_loop_unref(self->main_loop);
}

void
control_server_loop_start(ControlServerLoop *self)
{
  self->thread = g_thread_create((GThreadFunc) _thread, self, TRUE, NULL);
}

void
control_server_loop_stop(ControlServerLoop *self)
{
  iv_event_post(&self->stop_requested);
  g_thread_join(self->thread);
}

