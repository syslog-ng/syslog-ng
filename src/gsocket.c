#include "gsocket.h"
#include <arpa/inet.h>


typedef struct _GListenSource
{
  GSource super;
  GPollFD pollfd;
} GListenSource;

static gboolean
g_listen_prepare(GSource *source,
		 gint *timeout)
{
  GListenSource *self = (GListenSource *) source;

  self->pollfd.events = G_IO_IN;
  self->pollfd.revents = 0;
  *timeout = -1;
  return FALSE;
}

static gboolean
g_listen_check(GSource *source)
{
  GListenSource *self = (GListenSource *) source;

  return !!(self->pollfd.revents & (G_IO_IN | G_IO_ERR | G_IO_HUP));
}

static gboolean
g_listen_dispatch(GSource *source,
                  GSourceFunc callback,
                  gpointer user_data)
{
  return callback(user_data);
}

GSourceFuncs g_listen_source_funcs =
{
  g_listen_prepare,
  g_listen_check,
  g_listen_dispatch,
  NULL
};

GSource *
g_listen_source_new(gint fd)
{
  GListenSource *self = (GListenSource *) g_source_new(&g_listen_source_funcs, sizeof(GListenSource));

  self->pollfd.fd = fd;  
  g_source_set_priority(&self->super, LOG_PRIORITY_LISTEN);
  g_source_add_poll(&self->super, &self->pollfd);
  return &self->super;
}

typedef struct _GConnectSource
{
  GSource super;
  GPollFD pollfd;
} GConnectSource;

static gboolean
g_connect_prepare(GSource *source,
		  gint *timeout)
{
  GConnectSource *self = (GConnectSource *) source;

  self->pollfd.events = G_IO_OUT;
  self->pollfd.revents = 0;
  *timeout = -1;
  return FALSE;
}

static gboolean
g_connect_check(GSource *source)
{
  GConnectSource *self = (GConnectSource *) source;

  return !!(self->pollfd.revents & (G_IO_OUT | G_IO_ERR | G_IO_HUP));
}

static gboolean
g_connect_dispatch(GSource *source,
                   GSourceFunc callback,
                   gpointer user_data)
{
  callback(user_data);
  return FALSE;
}

GSourceFuncs g_connect_source_funcs =
{
  g_connect_prepare,
  g_connect_check,
  g_connect_dispatch,
  NULL
};

GSource *
g_connect_source_new(gint fd)
{
  GConnectSource *self = (GConnectSource *) g_source_new(&g_connect_source_funcs, sizeof(GConnectSource));

  self->pollfd.fd = fd;  
  g_source_set_priority(&self->super, LOG_PRIORITY_CONNECT);
  g_source_add_poll(&self->super, &self->pollfd);
  return &self->super;
}

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
