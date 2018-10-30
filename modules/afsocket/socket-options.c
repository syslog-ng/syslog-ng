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

static gboolean
_setup_receive_buffer(gint fd, gint so_rcvbuf)
{
  gint so_rcvbuf_set = 0;
  socklen_t sz = sizeof(so_rcvbuf_set);
  gint rc;

  rc = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &so_rcvbuf, sizeof(so_rcvbuf));
  if (rc < 0 ||
      getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &so_rcvbuf_set, &sz) < 0 ||
      sz != sizeof(so_rcvbuf_set) ||
      so_rcvbuf_set < so_rcvbuf)
    {
      msg_warning("The kernel refused to set the receive buffer (SO_RCVBUF) to the requested size, you probably need to adjust buffer related kernel parameters",
                  evt_tag_int("so_rcvbuf", so_rcvbuf),
                  evt_tag_int("so_rcvbuf_set", so_rcvbuf_set));
    }
  return TRUE;
}

static gboolean
_setup_send_buffer(gint fd, gint so_sndbuf)
{
  gint so_sndbuf_set = 0;
  socklen_t sz = sizeof(so_sndbuf_set);
  gint rc;

  rc = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &so_sndbuf, sizeof(so_sndbuf));

  if (rc < 0 ||
      getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &so_sndbuf_set, &sz) < 0 ||
      sz != sizeof(so_sndbuf_set) ||
      so_sndbuf_set < so_sndbuf)
    {
      msg_warning("The kernel refused to set the send buffer (SO_SNDBUF) to the requested size, you probably need to adjust buffer related kernel parameters",
                  evt_tag_int("so_sndbuf", so_sndbuf),
                  evt_tag_int("so_sndbuf_set", so_sndbuf_set));
    }
  return TRUE;
}

static gboolean
_setup_broadcast(gint fd)
{
  gint on = 1;
  setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
  return TRUE;
}

static gboolean
_setup_keepalive(gint fd)
{
  gint on = 1;
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
  return TRUE;
}

static gboolean
_setup_reuseport(gint fd)
{
#ifdef SO_REUSEPORT
  gint on = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) < 0)
    {
      msg_error("The kernel refused our SO_REUSEPORT setting, which should be supported by Linux 3.9+",
                evt_tag_error("error"));
      return FALSE;
    }
  return TRUE;
#else
  msg_error("You enabled so-reuseport(), but your platform does not support SO_REUSEPORT socket option, which should be supported on Linux 3.9+");
  return FALSE;
#endif
}

gboolean
socket_options_setup_socket_method(SocketOptions *self, gint fd, GSockAddr *bind_addr, AFSocketDirection dir)
{

  if (dir & AFSOCKET_DIR_RECV)
    {
      if (self->so_rcvbuf && !_setup_receive_buffer(fd, self->so_rcvbuf))
        return FALSE;
      if (self->so_reuseport && !_setup_reuseport(fd))
        return FALSE;
    }
  if (dir & AFSOCKET_DIR_SEND)
    {
      if (self->so_sndbuf && !_setup_send_buffer(fd, self->so_sndbuf))
        return FALSE;

      if (self->so_broadcast && !_setup_broadcast(fd))
        return FALSE;
    }
  if (self->so_keepalive && !_setup_keepalive(fd))
    return FALSE;
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
