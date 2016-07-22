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

#include "gsocket.h"

#include <errno.h>
#include <arpa/inet.h>

/**
 * g_inet_ntoa:
 * @buf:        store result in this buffer
 * @bufsize:    the available space in buf
 * @a:          address to convert.
 *
 * Thread friendly version of inet_ntoa(), converts an IP address to
 * human readable form. Returns: the address of buf
 **/
gchar *
g_inet_ntoa(char *buf, size_t bufsize, struct in_addr a)
{
  unsigned int ip = ntohl(a.s_addr);

  g_snprintf(buf, bufsize, "%d.%d.%d.%d",
             (ip & 0xff000000) >> 24,
             (ip & 0x00ff0000) >> 16,
             (ip & 0x0000ff00) >> 8,
             (ip & 0x000000ff));
  return buf;
}

gint
g_inet_aton(const char *buf, struct in_addr *a)
{
  return inet_aton(buf, a);
}

/**
 * g_bind:
 * @fd:         fd to bind
 * @addr:       address to bind to
 *
 * A thin interface around bind() using a GSockAddr structure for
 * socket address. It enables the NET_BIND_SERVICE capability (should be
 * in the permitted set.
 **/
GIOStatus
g_bind(int fd, GSockAddr *addr)
{
  GIOStatus rc;

  if (addr->sa_funcs && addr->sa_funcs->bind_prepare)
    addr->sa_funcs->bind_prepare(fd, addr);

  if (addr->sa_funcs && addr->sa_funcs->bind)
    rc = addr->sa_funcs->bind(fd, addr);
  else
    {
      if (addr && bind(fd, &addr->sa, addr->salen) < 0)
        {
          return G_IO_STATUS_ERROR;
        }
      rc = G_IO_STATUS_NORMAL;
    }
  return rc;
}

/**
 * g_accept:
 * @fd:         accept connection on this socket
 * @newfd:      fd of the accepted connection
 * @addr:       store the address of the client here
 *
 * Accept a connection on the given fd, returning the newfd and the
 * address of the client in a Zorp SockAddr structure.
 *
 *  Returns: glib style I/O error
 **/
GIOStatus
g_accept(int fd, int *newfd, GSockAddr **addr)
{
  char sabuf[1024];
  socklen_t salen = sizeof(sabuf);

  do
    {
      *newfd = accept(fd, (struct sockaddr *) sabuf, &salen);
    }
  while (*newfd == -1 && errno == EINTR);
  if (*newfd != -1)
    {
      *addr = g_sockaddr_new((struct sockaddr *) sabuf, salen);
    }
  else if (errno == EAGAIN)
    {
      return G_IO_STATUS_AGAIN;
    }
  else
    {
      return G_IO_STATUS_ERROR;
    }
  return G_IO_STATUS_NORMAL;
}

/**
 * g_connect:
 * @fd: socket to connect
 * @remote:  remote address
 *
 * Connect a socket using Zorp style GSockAddr structure.
 *
 * Returns: glib style I/O error
 **/
GIOStatus
g_connect(int fd, GSockAddr *remote)
{
  int rc;

  do
    {
      rc = connect(fd, &remote->sa, remote->salen);
    }
  while (rc == -1 && errno == EINTR);
  if (rc == -1)
    {
      if (errno == EAGAIN)
        return G_IO_STATUS_AGAIN;
      else
        return G_IO_STATUS_ERROR;
    }
  else
    {
      return G_IO_STATUS_NORMAL;
    }
}
