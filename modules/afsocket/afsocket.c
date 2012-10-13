/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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
#include "afsocket.h"
#include "messages.h"
#include "gsockaddr.h"
#include "gsocket.h"
#include "gprocess.h"
#include "misc.h"

#include <sys/socket.h>

gboolean
afsocket_setup_socket(gint fd, SocketOptions *sock_options, AFSocketDirection dir)
{
  gint rc;
  if (dir & AFSOCKET_DIR_RECV)
    {
      if (sock_options->so_rcvbuf)
        {
          gint so_rcvbuf_set = 0;
          socklen_t sz = sizeof(so_rcvbuf_set);

          rc = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &sock_options->so_rcvbuf, sizeof(sock_options->so_rcvbuf));
          if (rc < 0 ||
              getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &so_rcvbuf_set, &sz) < 0 ||
              sz != sizeof(so_rcvbuf_set) ||
              so_rcvbuf_set < sock_options->so_rcvbuf)
            {
              msg_warning("The kernel refused to set the receive buffer (SO_RCVBUF) to the requested size, you probably need to adjust buffer related kernel parameters",
                          evt_tag_int("so_rcvbuf", sock_options->so_rcvbuf),
                          evt_tag_int("so_rcvbuf_set", so_rcvbuf_set),
                          NULL);
            }
        }
    }
  if (dir & AFSOCKET_DIR_SEND)
    {
      if (sock_options->so_sndbuf)
        {
          gint so_sndbuf_set = 0;
          socklen_t sz = sizeof(so_sndbuf_set);

          rc = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sock_options->so_sndbuf, sizeof(sock_options->so_sndbuf));

          if (rc < 0 ||
              getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &so_sndbuf_set, &sz) < 0 ||
              sz != sizeof(so_sndbuf_set) ||
              so_sndbuf_set < sock_options->so_sndbuf)
            {
              msg_warning("The kernel refused to set the send buffer (SO_SNDBUF) to the requested size, you probably need to adjust buffer related kernel parameters",
                          evt_tag_int("so_sndbuf", sock_options->so_sndbuf),
                          evt_tag_int("so_sndbuf_set", so_sndbuf_set),
                          NULL);
            }
        }
      if (sock_options->so_broadcast)
        setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &sock_options->so_broadcast, sizeof(sock_options->so_broadcast));
    }
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &sock_options->so_keepalive, sizeof(sock_options->so_keepalive));
  return TRUE;
}

gboolean
afsocket_open_socket(GSockAddr *bind_addr, gint sock_type, gint sock_protocol, int *fd)
{
  gint sock;

  sock = socket(bind_addr->sa.sa_family, sock_type, sock_protocol);

  if (sock != -1)
    {
      cap_t saved_caps;

      g_fd_set_nonblock(sock, TRUE);
      g_fd_set_cloexec(sock, TRUE);
      saved_caps = g_process_cap_save();
      g_process_cap_modify(CAP_NET_BIND_SERVICE, TRUE);
      g_process_cap_modify(CAP_DAC_OVERRIDE, TRUE);
      if (g_bind(sock, bind_addr) != G_IO_STATUS_NORMAL)
        {
          gchar buf[256];

          g_process_cap_restore(saved_caps);
          msg_error("Error binding socket",
                    evt_tag_str("addr", g_sockaddr_format(bind_addr, buf, sizeof(buf), GSA_FULL)),
                    evt_tag_errno(EVT_TAG_OSERROR, errno),
                    NULL);
          close(sock);
          return FALSE;
        }
      g_process_cap_restore(saved_caps);

      *fd = sock;
      return TRUE;
    }
  else
    {
      msg_error("Error creating socket",
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return FALSE;
    }
}
