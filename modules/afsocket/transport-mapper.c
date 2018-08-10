/*
 * Copyright (c) 2002-2013 Balabit
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

#include "transport-mapper.h"
#include "gprocess.h"
#include "gsockaddr.h"
#include "gsocket.h"
#include "messages.h"
#include "fdhelpers.h"
#include "transport/transport-socket.h"

#include <errno.h>
#include <unistd.h>

static gboolean
transport_mapper_privileged_bind(gint sock, GSockAddr *bind_addr)
{
  cap_t saved_caps;
  GIOStatus status;

  saved_caps = g_process_cap_save();
  g_process_enable_cap(CAP_NET_BIND_SERVICE);
  g_process_enable_cap(CAP_DAC_OVERRIDE);

  status = g_bind(sock, bind_addr);

  g_process_cap_restore(saved_caps);
  return status == G_IO_STATUS_NORMAL;
}

gboolean
transport_mapper_open_socket(TransportMapper *self,
                             SocketOptions *socket_options,
                             GSockAddr *bind_addr,
                             AFSocketDirection dir,
                             int *fd)
{
  gint sock;

  sock = socket(self->address_family, self->sock_type, self->sock_proto);
  if (sock < 0)
    {
      msg_error("Error creating socket",
                evt_tag_error(EVT_TAG_OSERROR));
      goto error;
    }

  g_fd_set_nonblock(sock, TRUE);
  g_fd_set_cloexec(sock, TRUE);

  if (!socket_options_setup_socket(socket_options, sock, bind_addr, dir))
    goto error_close;

  if (!transport_mapper_privileged_bind(sock, bind_addr))
    {
      gchar buf[256];

      msg_error("Error binding socket",
                evt_tag_str("addr", g_sockaddr_format(bind_addr, buf, sizeof(buf), GSA_FULL)),
                evt_tag_error(EVT_TAG_OSERROR));
      goto error_close;
    }

  *fd = sock;
  return TRUE;

error_close:
  close(sock);
error:
  *fd = -1;
  return FALSE;
}

gboolean
transport_mapper_apply_transport_method(TransportMapper *self, GlobalConfig *cfg)
{
  return TRUE;
}

LogTransport *
transport_mapper_construct_log_transport_method(TransportMapper *self, gint fd)
{
  if (self->sock_type == SOCK_DGRAM)
    return log_transport_dgram_socket_new(fd);
  else
    return log_transport_stream_socket_new(fd);
}

void
transport_mapper_set_transport(TransportMapper *self, const gchar *transport)
{
  g_free(self->transport);
  self->transport = g_strdup(transport);
}

void
transport_mapper_set_address_family(TransportMapper *self, gint address_family)
{
  self->address_family = address_family;
}

void
transport_mapper_free_method(TransportMapper *self)
{
  g_free(self->transport);
}

void
transport_mapper_init_instance(TransportMapper *self, const gchar *transport)
{
  self->transport = g_strdup(transport);
  self->address_family = -1;
  self->sock_type = -1;
  self->free_fn = transport_mapper_free_method;
  self->apply_transport = transport_mapper_apply_transport_method;
  self->construct_log_transport = transport_mapper_construct_log_transport_method;
}

void
transport_mapper_free(TransportMapper *self)
{
  if (self->free_fn)
    self->free_fn(self);

  g_free(self);
}
