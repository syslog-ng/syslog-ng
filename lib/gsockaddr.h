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

#ifndef G_SOCKADDR_H_INCLUDED
#define G_SOCKADDR_H_INCLUDED

#include "syslog-ng.h"
#include "atomic.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

/* sockaddr public interface */

#define MAX_SOCKADDR_STRING 64

#define GSA_FULL    0
#define GSA_ADDRESS_ONLY 1

typedef struct _GSockAddrFuncs GSockAddrFuncs;

typedef struct _GSockAddr
{
  GAtomicCounter refcnt;
  guint32 flags;
  GSockAddrFuncs *sa_funcs;
  int salen;
  struct sockaddr sa;
} GSockAddr;

struct _GSockAddrFuncs
{
  GIOStatus (*bind_prepare)(gint sock, GSockAddr *addr);
  GIOStatus (*bind)(int sock, GSockAddr *addr);
  gchar   *(*format)(GSockAddr *addr, gchar *text, gulong n, gint format);
  guint16  (*get_port)          (GSockAddr *addr);
  void     (*set_port)          (GSockAddr *addr, guint16 port);
};

gchar *g_sockaddr_format(GSockAddr *a, gchar *text, gulong n, gint format);
guint16 g_sockaddr_get_port(GSockAddr *a);
void g_sockaddr_set_port(GSockAddr *a, guint16 port);
guint8 *g_sockaddr_get_address(GSockAddr *self, guint8 *buffer, socklen_t buffer_size);

GSockAddr *g_sockaddr_new(struct sockaddr *sa, int salen);
GSockAddr *g_sockaddr_new_from_peer_fd(gint fd);
GSockAddr *g_sockaddr_ref(GSockAddr *a);
void g_sockaddr_unref(GSockAddr *a);

static inline struct sockaddr *
g_sockaddr_get_sa(GSockAddr *self)
{
  return &self->sa;
}

gboolean g_sockaddr_inet_check(GSockAddr *a);
GSockAddr *g_sockaddr_inet_new(const gchar *ip, guint16 port);
GSockAddr *g_sockaddr_inet_new2(struct sockaddr_in *sin);

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


#if SYSLOG_NG_ENABLE_IPV6
gboolean g_sockaddr_inet6_check(GSockAddr *a);
GSockAddr *g_sockaddr_inet6_new(const gchar *ip, guint16 port);
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
 * @s: GSockAddrInet6 instance
 * @addr: new address
 *
 * This GSockAddrInet specific function sets the address part of the
 * address.
 **/
static inline void
g_sockaddr_inet6_set_address(GSockAddr *s, struct in6_addr *addr)
{
  g_sockaddr_inet6_get_sa(s)->sin6_addr = *addr;
}
#endif

GSockAddr *g_sockaddr_unix_new(const gchar *name);
GSockAddr *g_sockaddr_unix_new2(struct sockaddr_un *s_un, int sunlen);

#endif
