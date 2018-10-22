/*
 * Copyright (c) 2002-2010 Balabit
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#ifndef CONTROL_H_INCLUDED
#define CONTROL_H_INCLUDED

#include "syslog-ng.h"

typedef struct _ControlServer ControlServer;
typedef struct _ControlConnection ControlConnection;


/* the command function gets the command string and should return the
 * response to be returned to syslog-ng-ctl over the UNIX domain socket.  if
 * the response is NULL, no response is written to the peer and we are
 * blocked until control_connection_send_reply() is invoked in a future
 * callback. */
typedef GString *(*CommandFunction)(ControlConnection *cc, GString *, gpointer user_data);
typedef struct _ControlCommand
{
  const gchar *command_name;
  const gchar *description;
  CommandFunction func;
  gpointer user_data;
} ControlCommand;

gboolean control_command_start_with_command(const ControlCommand *cmd, const gchar *name);

#endif
