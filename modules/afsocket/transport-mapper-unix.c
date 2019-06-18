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
#include "transport-mapper-unix.h"
#include "transport-unix-socket.h"
#include "unix-credentials.h"
#include "stats/stats-registry.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


struct _TransportMapperUnix
{
  TransportMapper super;
  gchar *filename;
  gboolean pass_unix_credentials;
};

static LogTransport *
_create_log_transport(TransportMapper *s, gint fd)
{
  if (s->sock_type == SOCK_DGRAM)
    return log_transport_unix_dgram_socket_new(fd);
  else
    return log_transport_unix_stream_socket_new(fd);
}

static LogTransport *
_construct_log_transport(TransportMapper *s, gint fd)
{
  TransportMapperUnix *self = (TransportMapperUnix *) s;
  LogTransport *transport = _create_log_transport(s, fd);

  if (self->pass_unix_credentials)
    socket_set_pass_credentials(fd);

  return transport;
}

static TransportMapperUnix *
transport_mapper_unix_new_instance(const gchar *transport, gint sock_type)
{
  TransportMapperUnix *self = g_new0(TransportMapperUnix, 1);

  transport_mapper_init_instance(&self->super, transport);
  self->super.construct_log_transport = _construct_log_transport;
  self->super.address_family = AF_UNIX;
  self->super.sock_type = sock_type;
  return self;
}

void
transport_mapper_unix_set_pass_unix_credentials(TransportMapper *s, gboolean pass)
{
  TransportMapperUnix *self = (TransportMapperUnix *) s;
  self->pass_unix_credentials = pass;
}

TransportMapper *
transport_mapper_unix_dgram_new(void)
{
  TransportMapperUnix *self = transport_mapper_unix_new_instance("unix-dgram", SOCK_DGRAM);

  self->super.logproto = "dgram";
  self->super.stats_source = "unix-dgram";

  return &self->super;
}

TransportMapper *
transport_mapper_unix_stream_new(void)
{
  TransportMapperUnix *self = transport_mapper_unix_new_instance("unix-stream", SOCK_STREAM);

  self->super.logproto = "text";
  self->super.stats_source = "unix-stream";

  return &self->super;
}
