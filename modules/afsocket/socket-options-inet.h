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
#ifndef SOCKET_OPTIONS_INET_H_INCLUDED
#define SOCKET_OPTIONS_INET_H_INCLUDED

#include "socket-options.h"

typedef struct _SocketOptionsInet
{
  SocketOptions super;
  /* user settings */
  gint ip_ttl;
  gint ip_tos;
  gboolean ip_freebind;
  gint tcp_keepalive_time;
  gint tcp_keepalive_intvl;
  gint tcp_keepalive_probes;
  char *interface_name;
} SocketOptionsInet;

void socket_options_inet_set_interface_name(SocketOptionsInet *self, const gchar *interface);

SocketOptionsInet *socket_options_inet_new_instance(void);
SocketOptions *socket_options_inet_new(void);

#endif
