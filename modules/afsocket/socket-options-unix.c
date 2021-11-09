/*
 * Copyright (c) 2021 Balazs Scheidler <bazsi77@gmail.com>
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
#include "socket-options-unix.h"
#include "compat-unix-creds.h"

static gboolean
socket_options_unix_setup_socket(SocketOptions *s, gint fd, GSockAddr *addr, AFSocketDirection dir)
{
  SocketOptionsUnix *self = (SocketOptionsUnix *) s;

  if (!socket_options_setup_socket_method(s, fd, addr, dir))
    return FALSE;

  setsockopt_so_passcred(fd, self->so_passcred);

  return TRUE;
}

SocketOptions *
socket_options_unix_new(void)
{
  SocketOptionsUnix *self = g_new0(SocketOptionsUnix, 1);

  socket_options_init_instance(&self->super);
  self->super.setup_socket = socket_options_unix_setup_socket;
  self->so_passcred = TRUE;
  return &self->super;
}
