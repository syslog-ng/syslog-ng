/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#include "socket-options.h"
#include "messages.h"

#include <sys/types.h>
#include <sys/socket.h>

gboolean
socket_options_setup_socket_method(SocketOptions *self, gint fd, GSockAddr *bind_addr, AFSocketDirection dir)
{

  gint rc;
  if (dir & AFSOCKET_DIR_RECV)
    {
      if (self->so_rcvbuf)
        {
          gint so_rcvbuf_set = 0;
          socklen_t sz = sizeof(so_rcvbuf_set);

          rc = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &self->so_rcvbuf, sizeof(self->so_rcvbuf));
          if (rc < 0 ||
              getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &so_rcvbuf_set, &sz) < 0 ||
              sz != sizeof(so_rcvbuf_set) ||
              so_rcvbuf_set < self->so_rcvbuf)
            {
              msg_warning("The kernel refused to set the receive buffer (SO_RCVBUF) to the requested size, you probably need to adjust buffer related kernel parameters",
                          evt_tag_int("so_rcvbuf", self->so_rcvbuf),
                          evt_tag_int("so_rcvbuf_set", so_rcvbuf_set));
            }
        }
      if (self->so_reuseport)
        {
#ifdef SO_REUSEPORT
          if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &self->so_reuseport, sizeof(self->so_reuseport)) < 0)
            {
              msg_error("The kernel refused our SO_REUSEPORT setting, which should be supported by Linux 3.9+",
                        evt_tag_error("error"));
              return FALSE;
            }
#else
          msg_error("You enabled so-reuseport(), but your platform does not support SO_REUSEPORT socket option, which should be supported on Linux 3.9+");
          return FALSE;
#endif
        }
    }
  if (dir & AFSOCKET_DIR_SEND)
    {
      if (self->so_sndbuf)
        {
          gint so_sndbuf_set = 0;
          socklen_t sz = sizeof(so_sndbuf_set);

          rc = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &self->so_sndbuf, sizeof(self->so_sndbuf));

          if (rc < 0 ||
              getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &so_sndbuf_set, &sz) < 0 ||
              sz != sizeof(so_sndbuf_set) ||
              so_sndbuf_set < self->so_sndbuf)
            {
              msg_warning("The kernel refused to set the send buffer (SO_SNDBUF) to the requested size, you probably need to adjust buffer related kernel parameters",
                          evt_tag_int("so_sndbuf", self->so_sndbuf),
                          evt_tag_int("so_sndbuf_set", so_sndbuf_set));
            }
        }
      if (self->so_broadcast)
        setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &self->so_broadcast, sizeof(self->so_broadcast));
    }
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &self->so_keepalive, sizeof(self->so_keepalive));
  return TRUE;
}

void
socket_options_init_instance(SocketOptions *self)
{
  self->setup_socket = socket_options_setup_socket_method;
  self->free = g_free;
}

SocketOptions *
socket_options_new(void)
{
  SocketOptions *self = g_new0(SocketOptions, 1);

  socket_options_init_instance(self);
  return self;
}
