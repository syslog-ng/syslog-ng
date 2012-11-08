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

#ifndef AFSOCKET_H_INCLUDED
#define AFSOCKET_H_INCLUDED

#include "syslog-ng.h"
#include "gsockaddr.h"

typedef enum
{
  AFSOCKET_DIR_RECV = 0x01,
  AFSOCKET_DIR_SEND = 0x02,
} AFSocketDirection;

#define AFSOCKET_LOCAL               0x0004

#define AFSOCKET_SYSLOG_PROTOCOL     0x0008
#define AFSOCKET_KEEP_ALIVE          0x0100
#define AFSOCKET_REQUIRE_TLS         0x0200

typedef struct _SocketOptions
{
  gint so_sndbuf;
  gint so_rcvbuf;
  gint so_broadcast;
  gint so_keepalive;
} SocketOptions;

gboolean afsocket_setup_socket(gint fd, SocketOptions *sock_options, AFSocketDirection dir);
gboolean afsocket_open_socket(GSockAddr *bind_addr, gint sock_type, gint sock_protocol, int *fd);

#endif
