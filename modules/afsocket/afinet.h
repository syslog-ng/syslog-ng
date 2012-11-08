/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#ifndef AFINET_H_INCLUDED
#define AFINET_H_INCLUDED

#include "afsocket-source.h"
#include "afsocket-dest.h"

typedef struct _InetSocketOptions
{
  SocketOptions super;
  gint ip_ttl;
  gint ip_tos;
  gint tcp_keepalive_time;
  gint tcp_keepalive_intvl;
  gint tcp_keepalive_probes;
} InetSocketOptions;

void afinet_set_port(GSockAddr *addr, gchar *service, const gchar *proto);
gboolean afinet_setup_socket(gint fd, GSockAddr *addr, InetSocketOptions *sock_options, AFSocketDirection dir);



#endif
