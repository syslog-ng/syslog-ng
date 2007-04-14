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

#ifndef G_SOCKADDR_H_INCLUDED
#define G_SOCKADDR_H_INCLUDED

#include "syslog-ng.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>

/* sockaddr public interface */

typedef struct _GSockAddrFuncs GSockAddrFuncs;

typedef struct _GSockAddr
{
  gint refcnt;
  guint32 flags;
  GSockAddrFuncs *sa_funcs;
  int salen;
  struct sockaddr sa;
} GSockAddr;

struct _GSockAddrFuncs 
{
  GIOStatus (*sa_bind_prepare)   (int sock, GSockAddr *addr);
  GIOStatus (*sa_bind)		(int sock, GSockAddr *addr);
  gchar   *(*sa_format)         (GSockAddr *addr,   /* format to text form */
  				 gchar *text,
  				 gulong n);
  void     (*freefn)            (GSockAddr *addr);
};

GSockAddr *g_sockaddr_new(struct sockaddr *sa, int salen);
gchar *g_sockaddr_format(GSockAddr *a, gchar *text, gulong n);
GSockAddr *g_sockaddr_ref(GSockAddr *a);
void g_sockaddr_unref(GSockAddr *a);

gboolean g_sockaddr_inet_check(GSockAddr *a);
GSockAddr *g_sockaddr_inet_new(gchar *ip, guint16 port);
GSockAddr *g_sockaddr_inet_new2(struct sockaddr_in *sin);
GSockAddr *g_sockaddr_inet_range_new(gchar *ip, guint16 min_port, guint16 max_port);

static inline struct sockaddr *
g_sockaddr_get_sa(GSockAddr *self)
{
  return &self->sa;
}  

static inline struct sockaddr_in *
g_sockaddr_inet_get_sa(GSockAddr *s)
{
  g_assert(g_sockaddr_inet_check(s));
  
  return (struct sockaddr_in *) g_sockaddr_get_sa(s);
}
    

/**
 * g_sockaddr_inet_get_address:
 * @s: GSockAddrInet instance
 *
 * This GSockAddrInet specific function returns the address part of the
 * address.
 **/
static inline struct in_addr
g_sockaddr_inet_get_address(GSockAddr *s)
{
  return g_sockaddr_inet_get_sa(s)->sin_addr;
}

/**
 * g_sockaddr_inet_set_address:
 * @s: GSockAddrInet instance
 * @addr: new address
 *
 * This GSockAddrInet specific function returns the address part of the
 * address.
 **/
static inline void
g_sockaddr_inet_set_address(GSockAddr *s, struct in_addr addr)
{
  g_sockaddr_inet_get_sa(s)->sin_addr = addr;
}

/**
 * g_sockaddr_inet_get_port:
 * @s: GSockAddrInet instance
 *
 * This GSockAddrInet specific function returns the port part of the
 * address.
 *
 * Returns: the port in host byte order
 *
 **/
static inline guint16
g_sockaddr_inet_get_port(GSockAddr *s)
{
  return ntohs(g_sockaddr_inet_get_sa(s)->sin_port);
}

/**
 * g_sockaddr_inet_set_port:
 * @s: GSockAddrInet instance
 * @port: new port in host byte order
 *
 *
 **/
static inline void
g_sockaddr_inet_set_port(GSockAddr *s, guint16 port)
{
  g_sockaddr_inet_get_sa(s)->sin_port = htons(port);
}



#if ENABLE_IPV6
gboolean g_sockaddr_inet6_check(GSockAddr *a);
GSockAddr *g_sockaddr_inet6_new(gchar *ip, guint16 port);
GSockAddr *g_sockaddr_inet6_new2(struct sockaddr_in6 *sin6);

static inline struct sockaddr_in6 *
g_sockaddr_inet6_get_sa(GSockAddr *s)
{
  g_assert(g_sockaddr_inet6_check(s));
  
  return (struct sockaddr_in6 *) g_sockaddr_get_sa(s);
}

/**
 * g_sockaddr_inet6_get_address:
 * @s: GSockAddrInet instance
 *
 * This GSockAddrInet specific function returns the address part of the
 * address.
 **/
static inline struct in6_addr *
g_sockaddr_inet6_get_address(GSockAddr *s)
{
  return &g_sockaddr_inet6_get_sa(s)->sin6_addr;
}

/**
 * g_sockaddr_inet6_set_address:
 * @s: GSockAddrInet instance
 * @addr: new address
 *
 * This GSockAddrInet specific function returns the address part of the
 * address.
 **/
static inline void
g_sockaddr_inet6_set_address(GSockAddr *s, struct in6_addr *addr)
{
  g_sockaddr_inet6_get_sa(s)->sin6_addr = *addr;
}

/**
 * g_sockaddr_inet6_get_port:
 * @s: GSockAddrInet instance
 *
 * This GSockAddrInet specific function returns the port part of the
 * address.
 *
 * Returns: the port in host byte order
 *
 **/
static inline guint16
g_sockaddr_inet6_get_port(GSockAddr *s)
{
  return ntohs(g_sockaddr_inet6_get_sa(s)->sin6_port);
}

/**
 * g_sockaddr_inet6_set_port:
 * @s: GSockAddrInet instance
 * @port: new port in host byte order
 *
 *
 **/
static inline void
g_sockaddr_inet6_set_port(GSockAddr *s, guint16 port)
{
  g_sockaddr_inet6_get_sa(s)->sin6_port = htons(port);
}
#endif

GSockAddr *g_sockaddr_unix_new(gchar *name);
GSockAddr *g_sockaddr_unix_new2(struct sockaddr_un *s_un, int sunlen);

GIOStatus g_bind(int fd, GSockAddr *addr);
GIOStatus g_accept(int fd, int *newfd, GSockAddr **addr);
GIOStatus g_connect(int fd, GSockAddr *remote);
gchar *g_inet_ntoa(char *buf, size_t bufsize, struct in_addr a);
gint g_inet_aton(char *buf, struct in_addr *a);

#endif
