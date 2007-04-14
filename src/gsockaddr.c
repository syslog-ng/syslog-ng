/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gsockaddr.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

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
g_inet_aton(char *buf, struct in_addr *a)
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
  
  if (addr->sa_funcs && addr->sa_funcs->sa_bind_prepare)
    addr->sa_funcs->sa_bind_prepare(fd, addr);

  if (addr->sa_funcs && addr->sa_funcs->sa_bind)
    rc = addr->sa_funcs->sa_bind(fd, addr);
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
      return G_IO_STATUS_ERROR;
    }
  else
    {
      return G_IO_STATUS_NORMAL;
    }
}

/* general GSockAddr functions */


/**
 * g_sockaddr_new:
 *  @sa: libc sockaddr * pointer to convert
 *  @salen: size of sa
 *
 * General function to allocate and initialize a GSockAddr structure,
 * and convert a libc style sockaddr * pointer to our representation.
 *
 * Returns: a GSockAddr instance or NULL if failure
 *
 **/
GSockAddr *
g_sockaddr_new(struct sockaddr *sa, int salen)
{
  GSockAddr *addr = NULL;
  
  switch (sa->sa_family)
    {
#if ENABLE_IPV6
    case AF_INET6:
      if (salen >= sizeof(struct sockaddr_in6))
        addr = g_sockaddr_inet6_new2((struct sockaddr_in6 *) sa);
      break;
#endif
    case AF_INET:
      if (salen == sizeof(struct sockaddr_in))
        addr = g_sockaddr_inet_new2((struct sockaddr_in *) sa);
      break;
    case AF_UNIX:
      addr = g_sockaddr_unix_new2((struct sockaddr_un *) sa, salen);
      break;
    default:
      /*LOG
        This message indicates an internal error, Zorp tries to use an
        address family it doesn't support.
       */
      g_error("Unsupported socket family in g_sockaddr_new(); family='%d'", sa->sa_family);
      break;
    }
  return addr;
}

/**
 * g_sockaddr_format:
 * @a        instance pointer of a GSockAddr
 * @text     destination buffer
 * @n        the size of text
 *
 * Format a GSockAddr into human readable form, calls the format
 * virtual method of GSockAddr.
 *
 * Returns: text is filled with a human readable representation of a, and a
 * pointer to text is returned.
 *
 **/
char *
g_sockaddr_format(GSockAddr *a, gchar *text, gulong n)
{
  return a->sa_funcs->sa_format(a, text, n);
}

/*+

  Increment the reference count of a GSockAddr instance.

  Parameters:
    a         pointer to GSockAddr
  
  Returns:
    the same instance

  +*/
GSockAddr *
g_sockaddr_ref(GSockAddr *a)
{
  if (a)
    a->refcnt++;
  return a;
}

/*+

  Decrement the reference count of a GSockAddr instance, and free if
  the refcnt reaches 0.

  Parameters:
    a        GSockAddr instance

  Returns:
    none

  +*/
void 
g_sockaddr_unref(GSockAddr *a)
{
  /* FIXME: maybe add a callback to funcs table */
  if (a) 
    {
      if (--a->refcnt == 0)
        {
          if (!a->sa_funcs->freefn)
            g_free(a);
          else
            a->sa_funcs->freefn(a);
        }
    }
}

/* AF_INET socket address */
/*+

  GSockAddrInet is an implementation of the GSockAddr interface,
  encapsulating an IPv4 host address and port number.

  +*/
typedef struct _GSockAddrInet
{
  gint refcnt;
  guint32 flags;
  GSockAddrFuncs *sa_funcs;
  int salen;
  struct sockaddr_in sin;
} GSockAddrInet;

/*+ private function to prepare an IPv4 style bind, e.g. set
  SO_REUSEADDR +*/
static GIOStatus 
g_sockaddr_inet_bind_prepare(int sock, GSockAddr *addr)
{
  int tmp = 1;
  
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(tmp));
  return G_IO_STATUS_NORMAL;
}

/*+ format an IPv4 address into human readable form */
gchar *
g_sockaddr_inet_format(GSockAddr *addr, gchar *text, gulong n)
{
  GSockAddrInet *inet_addr = (GSockAddrInet *) addr;
  char buf[32];
  
  g_snprintf(text, n, "AF_INET(%s:%d)", 
	     g_inet_ntoa(buf, sizeof(buf), inet_addr->sin.sin_addr), 
             htons(inet_addr->sin.sin_port));
  return text;
}

void
g_sockaddr_inet_free(GSockAddr *addr)
{
  g_free(addr);
}

static GSockAddrFuncs inet_sockaddr_funcs = 
{
  g_sockaddr_inet_bind_prepare,
  NULL,
  g_sockaddr_inet_format,
  g_sockaddr_inet_free
};

gboolean
g_sockaddr_inet_check(GSockAddr *a)
{
  return a->sa_funcs == &inet_sockaddr_funcs;
}

/*+

  Allocate and initialize an IPv4 socket address.

  Parameters:
    ip          text representation of an IP address
    port        port number in host byte order

  Returns:
    the new instance

  +*/
GSockAddr *
g_sockaddr_inet_new(gchar *ip, guint16 port)
{
  GSockAddrInet *addr = NULL;
  struct in_addr ina;

  if (inet_aton(ip, &ina))
    {
      addr = g_new0(GSockAddrInet, 1);
  
      addr->refcnt = 1;
      addr->flags = 0;
      addr->salen = sizeof(struct sockaddr_in);
      addr->sin.sin_family = AF_INET;
      addr->sin.sin_port = htons(port);
      addr->sin.sin_addr = ina;
      addr->sa_funcs = &inet_sockaddr_funcs;
    }
  return (GSockAddr *) addr;
}

/*+

  Allocate and initialize an IPv4 socket address using libc sockaddr *
  structure.

  Parameters:
    sin         the sockaddr_in structure to convert

  Returns:
    the allocated instance.

  +*/
GSockAddr *
g_sockaddr_inet_new2(struct sockaddr_in *sin)
{
  GSockAddrInet *addr = g_new0(GSockAddrInet, 1);
  
  addr->refcnt = 1;
  addr->flags = 0;
  addr->salen = sizeof(struct sockaddr_in);
  addr->sin = *sin;
  addr->sa_funcs = &inet_sockaddr_funcs;
  
  return (GSockAddr *) addr;
}

/*+ Similar to GSockAddrInet, but binds to a port in the given range +*/

/* 
 * NOTE: it is assumed that it is safe to cast from GSockAddrInetRange to
 * GSockAddrInet
 */
typedef struct _GSockAddrInetRange
{
  gint refcnt;
  guint32 flags;
  GSockAddrFuncs *sa_funcs;
  int salen;
  struct sockaddr_in sin;
  gpointer options;
  guint options_length;
  guint16 min_port, max_port, last_port;
} GSockAddrInetRange;

static GIOStatus
g_sockaddr_inet_range_bind(int sock, GSockAddr *a)
{
  GSockAddrInetRange *addr = (GSockAddrInetRange *) a;
  int port;
  
  if (addr->min_port > addr->max_port)
    {
      /*LOG
        This message indicates that SockAddrInetRange was given incorrect
        parameters, the allowed min_port is greater than max_port.
       */
      g_error("SockAddrInetRange, invalid range given; min_port='%d', max_port='%d'", addr->min_port, addr->max_port);
      return G_IO_STATUS_ERROR;
    }
  for (port = addr->last_port; port <= addr->max_port; port++)
    {
      /* attempt to bind */
      addr->sin.sin_port = htons(port);
      if (bind(sock, (struct sockaddr *) &addr->sin, addr->salen) == 0)
        {
          addr->last_port = port + 1;
          return G_IO_STATUS_NORMAL;
        }
    }
  for (port = addr->min_port; port <= addr->max_port; port++)
    {
      /* attempt to bind */
      addr->sin.sin_port = htons(port);
      if (bind(sock, (struct sockaddr *) &addr->sin, addr->salen) == 0)
        {
          addr->last_port = port + 1;
          return G_IO_STATUS_NORMAL;
        }
    }
  addr->last_port = addr->min_port;
  return G_IO_STATUS_ERROR;
}

static GSockAddrFuncs inet_range_sockaddr_funcs = 
{
  NULL,
  g_sockaddr_inet_range_bind,
  g_sockaddr_inet_format,
  g_sockaddr_inet_free,
};

GSockAddr *
g_sockaddr_inet_range_new(gchar *ip, guint16 min_port, guint16 max_port)
{
  GSockAddrInetRange *addr = g_new0(GSockAddrInetRange, 1);
  
  addr->refcnt = 1;
  addr->flags = 0;
  addr->salen = sizeof(struct sockaddr_in);
  addr->sin.sin_family = AF_INET;
  inet_aton(ip, &addr->sin.sin_addr);
  addr->sin.sin_port = 0;
  addr->sa_funcs = &inet_range_sockaddr_funcs;
  if (max_port > min_port)
    {
      addr->last_port = (rand() % (max_port - min_port)) + min_port;
    }
  addr->min_port = min_port;
  addr->max_port = max_port;
  
  return (GSockAddr *) addr;
}


#if ENABLE_IPV6
/* AF_INET6 socket address */
/*+

  GSockAddrInet6 is an implementation of the GSockAddr interface,
  encapsulating an IPv6 host address and port number.

  +*/
typedef struct _GSockAddrInet6
{
  gint refcnt;
  guint32 flags;
  GSockAddrFuncs *sa_funcs;
  int salen;
  struct sockaddr_in6 sin6;
} GSockAddrInet6;

/*+ format an IPv6 address into human readable form */
static gchar *
g_sockaddr_inet6_format(GSockAddr *addr, gchar *text, gulong n)
{
  GSockAddrInet6 *self = (GSockAddrInet6 *) addr;
  char buf[64];
  
  inet_ntop(AF_INET6, &self->sin6.sin6_addr, buf, sizeof(buf));
  g_snprintf(text, n, "AF_INET6(%s:%d)", 
	     buf, 
             htons(self->sin6.sin6_port));
  return text;
}

static void
g_sockaddr_inet6_free(GSockAddr *addr)
{
  g_free(addr);
}

static GSockAddrFuncs inet6_sockaddr_funcs = 
{
  g_sockaddr_inet_bind_prepare,
  NULL,
  g_sockaddr_inet6_format,
  g_sockaddr_inet6_free
};

gboolean
g_sockaddr_inet6_check(GSockAddr *a)
{
  return a->sa_funcs == &inet6_sockaddr_funcs;
}


/*+

  Allocate and initialize an IPv6 socket address.

  Parameters:
    ip          text representation of an IP address
    port        port number in host byte order

  Returns:
    the new instance

  +*/
GSockAddr *
g_sockaddr_inet6_new(gchar *ip, guint16 port)
{
  GSockAddrInet6 *addr = g_new0(GSockAddrInet6, 1);
  
  addr->refcnt = 1;
  addr->flags = 0;
  addr->salen = sizeof(struct sockaddr_in6);
  addr->sin6.sin6_family = AF_INET6;
  inet_pton(AF_INET6, ip, &addr->sin6.sin6_addr);
  addr->sin6.sin6_port = htons(port);
  addr->sa_funcs = &inet6_sockaddr_funcs;
  
  return (GSockAddr *) addr;
}


/*+

  Allocate and initialize an IPv6 socket address using libc sockaddr *
  structure.

  Parameters:
    sin         the sockaddr_in6 structure to convert

  Returns:
    the allocated instance.

  +*/
GSockAddr *
g_sockaddr_inet6_new2(struct sockaddr_in6 *sin6)
{
  GSockAddrInet6 *addr = g_new0(GSockAddrInet6, 1);
  
  addr->refcnt = 1;
  addr->flags = 0;
  addr->salen = sizeof(struct sockaddr_in6);
  addr->sin6 = *sin6;
  addr->sa_funcs = &inet6_sockaddr_funcs;
  
  return (GSockAddr *) addr;
}

#endif

/* AF_UNIX socket address */

/*+

  The GSockAddrUnix class is an implementation of the GSockAddr
  interface encapsulating AF_UNIX domain socket names.

  +*/
typedef struct _GSockAddrUnix
{
  gint refcnt;
  guint32 flags;
  GSockAddrFuncs *sa_funcs;
  int salen;
  struct sockaddr_un saun;
} GSockAddrUnix;

static GIOStatus g_sockaddr_unix_bind_prepare(int sock, GSockAddr *addr);
static gchar *g_sockaddr_unix_format(GSockAddr *addr, gchar *text, gulong n);

static GSockAddrFuncs unix_sockaddr_funcs = 
{
  g_sockaddr_unix_bind_prepare,
  NULL,
  g_sockaddr_unix_format
};

/* anonymous if name == NULL */

/*+

  Allocate and initialize a GSockAddrUnix instance.

  Parameters:
    name              socket filename

  Returns:
    the new instance

  +*/
GSockAddr *
g_sockaddr_unix_new(gchar *name)
{
  GSockAddrUnix *addr = g_new0(GSockAddrUnix, 1);
  
  addr->refcnt = 1;
  addr->flags = 0;
  addr->sa_funcs = &unix_sockaddr_funcs;
  addr->saun.sun_family = AF_UNIX;
  if (name)
    {
      strncpy(addr->saun.sun_path, name, sizeof(addr->saun.sun_path) - 1);
      addr->saun.sun_path[sizeof(addr->saun.sun_path) - 1] = 0;
      addr->salen = sizeof(addr->saun) - sizeof(addr->saun.sun_path) + strlen(addr->saun.sun_path) + 1;
    }
  else
    {
      addr->saun.sun_path[0] = 0;
      addr->salen = 2;
    }
  return (GSockAddr *) addr;
}

/*+

  Allocate and initialize a GSockAddrUnix instance, using libc
  sockaddr_un strucutre.

  Parameters:
    saun        sockaddr_un structure to convert
    sunlen      size of saun

  Returns:
    the new instance

  +*/
GSockAddr *
g_sockaddr_unix_new2(struct sockaddr_un *saun, int sunlen)
{
  GSockAddrUnix *addr = g_new0(GSockAddrUnix, 1);
  
  addr->refcnt = 1;
  addr->flags = 0;
  addr->sa_funcs = &unix_sockaddr_funcs;
  addr->salen = sunlen;
  addr->saun = *saun;
  return (GSockAddr *) addr;
}

/*+ private function to prepare a bind on AF_UNIX sockets, e.g. unlink
  the socket if it exists and is a socket. +*/
static GIOStatus 
g_sockaddr_unix_bind_prepare(int sock, GSockAddr *addr)
{
  GSockAddrUnix *unix_addr = (GSockAddrUnix *) addr;
  struct stat st;
  
  if (unix_addr->saun.sun_path[0] == 0)
    return G_IO_STATUS_NORMAL;

  if (stat(unix_addr->saun.sun_path, &st) == -1 ||
      !S_ISSOCK(st.st_mode))
    return G_IO_STATUS_NORMAL;
  
  unlink(unix_addr->saun.sun_path);  
  return G_IO_STATUS_NORMAL;
}

/*+ Convert a GSockAddrUnix into human readable form. +*/
gchar *
g_sockaddr_unix_format(GSockAddr *addr, gchar *text, gulong n)
{
  GSockAddrUnix *unix_addr = (GSockAddrUnix *) addr;
  
  g_snprintf(text, n, "AF_UNIX(%s)", 
             unix_addr->saun.sun_path[0] ? unix_addr->saun.sun_path
                                          : "anonymous");
  return text;
}

