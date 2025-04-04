/*
 * Copyright (c) 2023 One Identity LLC.
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#ifndef TRANSPORT_SOCKET_H_INCLUDED
#define TRANSPORT_SOCKET_H_INCLUDED 1

#include "logtransport.h"

typedef struct _LogTransportSocket LogTransportSocket;
struct _LogTransportSocket
{
  LogTransport super;
  gint address_family;
  gint proto;
  void (*parse_cmsg)(LogTransportSocket *self, struct cmsghdr *cmsg, LogTransportAuxData *aux);
};

void log_transport_socket_parse_cmsg_method(LogTransportSocket *s, struct cmsghdr *cmsg, LogTransportAuxData *aux);
gssize log_transport_socket_read_method(LogTransport *s, gpointer buf, gsize buflen, LogTransportAuxData *aux);

void log_transport_dgram_socket_init_instance(LogTransportSocket *self, gint fd);
LogTransport *log_transport_dgram_socket_new(gint fd);

void log_transport_stream_socket_init_instance(LogTransportSocket *self, gint fd);
void log_transport_stream_socket_free_method(LogTransport *s);
LogTransport *log_transport_stream_socket_new(gint fd);

#endif
