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

GSockAddr *g_sockaddr_inet_new(gchar *ip, guint16 port);
GSockAddr *g_sockaddr_inet_new2(struct sockaddr_in *sin);
GSockAddr *g_sockaddr_inet_range_new(gchar *ip, guint16 min_port, guint16 max_port);

#if ENABLE_IPV6
GSockAddr *g_sockaddr_inet6_new(gchar *ip, guint16 port);
GSockAddr *g_sockaddr_inet6_new2(struct sockaddr_in6 *sin6);

#endif

GSockAddr *g_sockaddr_unix_new(gchar *name);
GSockAddr *g_sockaddr_unix_new2(struct sockaddr_un *s_un, int sunlen);

GIOStatus g_bind(int fd, GSockAddr *addr);
GIOStatus g_accept(int fd, int *newfd, GSockAddr **addr);
GIOStatus g_connect(int fd, GSockAddr *remote);
gchar *g_inet_ntoa(char *buf, size_t bufsize, struct in_addr a);
gint g_inet_aton(char *buf, struct in_addr *a);

#endif
