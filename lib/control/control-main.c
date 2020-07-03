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

struct _ControlServerLoop
{
  ControlServer *control_server;
};

ControlServerLoop *
control_init(const gchar *control_name)
{
  ControlServerLoop *self = g_new0(ControlServerLoop, 1);
  self->control_server = control_server_new(control_name);
  control_server_start(self->control_server);

  return self;
}

void
control_deinit(ControlServerLoop *self)
{
  if (!self)
    return;

  reset_control_command_list();
  if (self && self->control_server)
    control_server_free(self->control_server);
  g_free(self);
}
