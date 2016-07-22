/*
 * Copyright (c) 2002-2010 Balabit
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#ifndef G_SOCKET_H_INCLUDED
#define G_SOCKET_H_INCLUDED

#include "syslog-ng.h"
#include "gsockaddr.h"

GIOStatus g_bind(int fd, GSockAddr *addr);
GIOStatus g_accept(int fd, int *newfd, GSockAddr **addr);
GIOStatus g_connect(int fd, GSockAddr *remote);
gchar *g_inet_ntoa(char *buf, size_t bufsize, struct in_addr a);
gint g_inet_aton(const char *buf, struct in_addr *a);

#endif
