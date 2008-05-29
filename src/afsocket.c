/*
 * Copyright (c) 2002-2007 BalaBit IT Ltd, Budapest, Hungary                    
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
  
#include "afsocket.h"
#include "messages.h"
#include "driver.h"
#include "misc.h"
#include "logwriter.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>

#if ENABLE_TCP_WRAPPER
#include <tcpd.h>
int allow_severity = 0;
int deny_severity = 0;
#endif


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

static GSource *
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

static GSource *
g_connect_source_new(gint fd)
{
  GConnectSource *self = (GConnectSource *) g_source_new(&g_connect_source_funcs, sizeof(GConnectSource));

  self->pollfd.fd = fd;  
  g_source_set_priority(&self->super, LOG_PRIORITY_CONNECT);
  g_source_add_poll(&self->super, &self->pollfd);
  return &self->super;
}

typedef struct _AFSocketSourceConnection
{
  LogPipe super;
  struct _AFSocketSourceDriver *owner;
  LogPipe *reader;
  int sock;
  GSockAddr *peer_addr;
} AFSocketSourceConnection;

static void afsocket_sd_close_connection(AFSocketSourceDriver *self, AFSocketSourceConnection *sc);

gboolean
afsocket_setup_socket(gint fd, SocketOptions *sock_options, AFSocketDirection dir)
{
  if (dir & AFSOCKET_DIR_RECV)
    {
      if (sock_options->rcvbuf)
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &sock_options->rcvbuf, sizeof(sock_options->rcvbuf));
    }
  if (dir & AFSOCKET_DIR_SEND)
    {
      if (sock_options->sndbuf)
        setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sock_options->sndbuf, sizeof(sock_options->sndbuf));
      if (sock_options->broadcast)
        setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &sock_options->broadcast, sizeof(sock_options->broadcast));
    }
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &sock_options->keepalive, sizeof(sock_options->keepalive));
  return TRUE;
}

static gboolean
afsocket_open_socket(GSockAddr *bind_addr, int stream_or_dgram, int *fd)
{
  gint sock;
  
  if (stream_or_dgram)
    sock = socket(bind_addr->sa.sa_family, SOCK_STREAM, 0);
  else
    sock = socket(bind_addr->sa.sa_family, SOCK_DGRAM, 0);
    
  g_fd_set_nonblock(sock, TRUE);
  g_fd_set_cloexec(sock, TRUE);
  if (sock != -1)
    {
      if (g_bind(sock, bind_addr) != G_IO_STATUS_NORMAL)
        {
          gchar buf[256];
          
          msg_error("Error binding socket",
                    evt_tag_str("addr", g_sockaddr_format(bind_addr, buf, sizeof(buf))),
                    evt_tag_errno(EVT_TAG_OSERROR, errno),
                    NULL);
          close(sock);
          return FALSE;
        }
      
      *fd = sock;
      return TRUE;
    }
  else
    {
      msg_error("Error creating socket",
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return FALSE;
    }
}

static gboolean
afsocket_sc_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;
  gint read_flags = (self->owner->flags & AFSOCKET_DGRAM) ? FR_RECV : 0;
  FDRead *reader;
  
  reader = fd_read_new(self->sock, read_flags);

  self->reader = log_reader_new(reader,
                                ((self->owner->flags & AFSOCKET_LOCAL) ? LR_LOCAL : 0) | 
                                ((self->owner->flags & AFSOCKET_DGRAM) ? LR_PKTTERM : 0),
                                s, &self->owner->reader_options);
  log_pipe_append(self->reader, s);
  log_pipe_init(self->reader, NULL, NULL);
  return TRUE;
}

static gboolean
afsocket_sc_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;

  self->owner->connections = g_list_remove(self->owner->connections, self);
  log_pipe_unref(&self->owner->super.super);
  self->owner = NULL;
  
  log_pipe_deinit(self->reader, NULL, NULL);
  log_pipe_unref(self->reader);
  self->reader = NULL;
  return TRUE;
}

static void
afsocket_sc_queue(LogPipe *s, LogMessage *msg, gint path_flags)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;
  
  if (!msg->saddr)
    {
      if (self->peer_addr)
        msg->saddr = g_sockaddr_ref(self->peer_addr);
    }
    
  log_pipe_queue(s->pipe_next, msg, path_flags);
}

static void
afsocket_sc_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;

  switch (notify_code)
    {
    case NC_CLOSE:
    case NC_READ_ERROR:
      {
        if (self->owner->flags & AFSOCKET_STREAM)
          afsocket_sd_close_connection(self->owner, self);
        break;
      }
    }
}

static void
afsocket_sc_set_owner(AFSocketSourceConnection *self, AFSocketSourceDriver *owner)
{
  if (self->reader)
    log_reader_set_options(self->reader, &owner->reader_options);
  log_drv_unref(&self->owner->super);
  log_drv_ref(&owner->super);
  self->owner = owner;
  
  log_pipe_append(&self->super, &owner->super.super);

}

static void 
afsocket_sc_free(LogPipe *s)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;

  
  g_assert(!self->reader);
  g_sockaddr_unref(self->peer_addr);
  g_free(self);
}

AFSocketSourceConnection *
afsocket_sc_new(AFSocketSourceDriver *owner, GSockAddr *peer_addr, int fd)
{
  AFSocketSourceConnection *self = g_new0(AFSocketSourceConnection, 1);

  log_pipe_init_instance(&self->super);  
  self->super.init = afsocket_sc_init;
  self->super.deinit = afsocket_sc_deinit;
  self->super.queue = afsocket_sc_queue;
  self->super.notify = afsocket_sc_notify;
  self->super.free_fn = afsocket_sc_free;
  log_drv_ref(&owner->super);
  self->owner = owner;

  owner->connections = g_list_prepend(owner->connections, self);

  self->peer_addr = g_sockaddr_ref(peer_addr);  
  self->sock = fd;
  return self;
}

void 
afsocket_sd_set_keep_alive(LogDriver *s, gint enable)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  
  if (enable)
    self->flags |= AFSOCKET_KEEP_ALIVE;
  else
    self->flags &= ~AFSOCKET_KEEP_ALIVE;
}

void 
afsocket_sd_set_max_connections(LogDriver *s, gint max_connections)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  
  self->max_connections = max_connections;
}


static inline gchar *
afsocket_sd_format_persist_name(AFSocketSourceDriver *self, gboolean listener_name)
{
  static gchar persist_name[128];
  gchar buf[64];
  
  g_snprintf(persist_name, sizeof(persist_name),
             listener_name ? "afsocket_sd_listen_fd(%s,%s)" : "afsocket_sd_connections(%s,%s)",
             !!(self->flags & AFSOCKET_STREAM) ? "stream" : "dgram",
             g_sockaddr_format(self->bind_addr, buf, sizeof(buf)));
  return persist_name;
}

gboolean 
afsocket_sd_process_connection(AFSocketSourceDriver *self, GSockAddr *peer_addr, gint fd)
{
#if ENABLE_TCP_WRAPPER
  if (peer_addr && (peer_addr->sa.sa_family == AF_INET
#if ENABLE_IPV6
                   || peer_addr->sa.sa_family == AF_INET6
#endif
     ))
    {
      struct request_info req;
   
      request_init(&req, RQ_DAEMON, "syslog-ng", RQ_FILE, fd, 0);
      fromhost(&req);
      if (hosts_access(&req) == 0)
        {
          gchar buf[256];
          
          msg_error("Syslog connection rejected by tcpd",
                    evt_tag_str("from", g_sockaddr_format(peer_addr, buf, sizeof(buf))),
                    NULL);
          close(fd);
          return TRUE;
        }
    }
  
#endif

  if (self->num_connections >= self->max_connections)
    {
      msg_error("Number of allowed concurrent connections exceeded",
                evt_tag_int("num", self->num_connections),
                evt_tag_int("max", self->max_connections),
                NULL);
      close(fd);
    }
  else
    {
      AFSocketSourceConnection *conn;
      
      self->num_connections++;
      conn = afsocket_sc_new(self, peer_addr, fd);
      log_pipe_init(&conn->super, NULL, NULL);
      log_pipe_append(&conn->super, &self->super.super);
    }
  return TRUE;
}

#define MAX_ACCEPTS_AT_A_TIME 30

static gboolean
afsocket_sd_accept(gpointer s)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  GSockAddr *peer_addr;
  gchar buf1[256], buf2[256];
  gint new_fd;
  gboolean res;
  int accepts = 0;
  
  while (accepts < MAX_ACCEPTS_AT_A_TIME)
    {
      GIOStatus status;
      
      status = g_accept(self->fd, &new_fd, &peer_addr);
      if (status == G_IO_STATUS_AGAIN)
        {
          /* no more connections to accept */
          break;
        }
      else if (status != G_IO_STATUS_NORMAL)
        {
          msg_error("Error accepting new connection",
                    evt_tag_errno(EVT_TAG_OSERROR, errno),
                    NULL);
          return TRUE;
        }
      if (self->setup_socket && !self->setup_socket(self, new_fd))
        {
          close(new_fd);
          return TRUE;
        }
        
      g_fd_set_nonblock(new_fd, TRUE);
      g_fd_set_cloexec(new_fd, TRUE);
        
      msg_verbose("Syslog connection accepted",
                  evt_tag_str("from", g_sockaddr_format(peer_addr, buf1, sizeof(buf1))),
                  evt_tag_str("to", g_sockaddr_format(self->bind_addr, buf2, sizeof(buf2))),
                  NULL);

      res = afsocket_sd_process_connection(self, peer_addr, new_fd);
      g_sockaddr_unref(peer_addr);
      if (!res)
        return FALSE;
      accepts++;
    }
  return TRUE;
}

static void
afsocket_sd_close_connection(AFSocketSourceDriver *self, AFSocketSourceConnection *sc)
{
  log_pipe_deinit(&sc->super, NULL, NULL);
  log_pipe_unref(&sc->super);
  self->num_connections--;
}

static void
afsocket_sd_kill_connection(AFSocketSourceConnection *sc)
{
  log_pipe_deinit(&sc->super, NULL, NULL);
  log_pipe_unref(&sc->super);
}

static void
afsocket_sd_kill_connection_list(GList *list)
{
  GList *l;
  for (l = list; l; l = g_list_next(l))
    afsocket_sd_kill_connection((AFSocketSourceConnection *) l->data);
}

gboolean
afsocket_sd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  gint sock;
  gboolean res = FALSE;
  
  if (!self->bind_addr)
    {
      msg_error("No bind address set;", NULL);
    }
  log_reader_options_init(&self->reader_options, cfg);
  
  /* fetch persistent connections first */  
  if ((self->flags & AFSOCKET_KEEP_ALIVE))
    {
      GList *p;

      self->connections = persist_config_fetch(persist, afsocket_sd_format_persist_name(self, FALSE));
      for (p = self->connections; p; p = p->next)
        {
          afsocket_sc_set_owner((AFSocketSourceConnection *) p->data, self);
        }
    }

  /* ok, we have connection list, check if we need to open a listener */
  sock = -1;
  if (self->flags & AFSOCKET_STREAM)
    {
      GSource *source;
      
      if (self->flags & AFSOCKET_KEEP_ALIVE)
        {
          /* NOTE: this assumes that fd 0 will never be used for listening fds,
           * main.c opens fd 0 so this assumption can hold */
          sock = GPOINTER_TO_UINT(persist_config_fetch(persist, afsocket_sd_format_persist_name(self, TRUE))) - 1;
        }

      if (sock == -1)
        {
          if (!afsocket_open_socket(self->bind_addr, !!(self->flags & AFSOCKET_STREAM), &sock))
            return self->super.optional;
        }
      
      /* set up listening source */
      if (listen(sock, self->listen_backlog) < 0)
        {
          msg_error("Error during listen()",
                    evt_tag_errno(EVT_TAG_OSERROR, errno),
                    NULL);
          close(sock);
          return FALSE;
        }
        
      if (self->setup_socket && !self->setup_socket(self, sock))
        {
          close(sock);
          return FALSE;
        }

      self->fd = sock;
      source = g_listen_source_new(self->fd);
      
      /* the listen_source references us, which is freed when the source is deleted */
      log_pipe_ref(s); 
      g_source_set_callback(source, afsocket_sd_accept, self, (GDestroyNotify) log_pipe_unref);
      self->source_id = g_source_attach(source, NULL);
      g_source_unref(source);
      res = TRUE;
    }
  else
    {
      if (!self->connections)
        {
          if (!afsocket_open_socket(self->bind_addr, !!(self->flags & AFSOCKET_STREAM), &sock))
            return self->super.optional;
        }
      self->fd = -1;

      if (!self->setup_socket(self, sock))
        {
          close(sock);
          return FALSE;
        }
      
      /* we either have self->connections != NULL, or sock contains a new fd */
      if (self->connections || afsocket_sd_process_connection(self, NULL, sock))
        res = TRUE;
    }
  return res;
}

static void
afsocket_sd_close_fd(gpointer value)
{
  gint fd = GPOINTER_TO_UINT(value) - 1;
  close(fd);
}

gboolean
afsocket_sd_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  
  if ((self->flags & AFSOCKET_KEEP_ALIVE) == 0 || !persist)
    {
      GList *p, *next;

      /* we don't store anything across HUPs */
      for (p = self->connections; p; p = next)
        {
          next = p->next;
          afsocket_sd_kill_connection((AFSocketSourceConnection *) p->data);
        }
        
      /* NOTE: we don't need to free the connection list, when a connection
       * is freed it is removed from the list automatically */
      
    }
  else
    {
      /* for AFSOCKET_STREAM source drivers this is a list, for
       * AFSOCKET_DGRAM this is a single connection */
      
      persist_config_add(persist, afsocket_sd_format_persist_name(self, FALSE), self->connections, (GDestroyNotify) afsocket_sd_kill_connection_list);
    }
  self->connections = NULL;

  if (self->flags & AFSOCKET_STREAM)
    {
      
      g_source_remove(self->source_id);
      self->source_id = 0;
      if ((self->flags & AFSOCKET_KEEP_ALIVE) == 0)
        {
          msg_verbose("Closing listener fd",
                      evt_tag_int("fd", self->fd),
                      NULL);
          close(self->fd);
        }
      else
        {
          /* NOTE: the fd is incremented by one when added to persistent config
           * as persist config cannot store NULL */

          persist_config_add(persist, afsocket_sd_format_persist_name(self, TRUE), GUINT_TO_POINTER(self->fd + 1), afsocket_sd_close_fd);
        }
    }
  else if (self->flags & AFSOCKET_DGRAM)
    {
      /* we don't need to close the listening fd here as we have a
       * single connection which will close it */
      
      ;
    }
  
  
  return TRUE;
}

static void
afsocket_sd_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  switch (notify_code)
    {
    case NC_CLOSE:
    case NC_READ_ERROR:
      {
        g_assert_not_reached();
        break;
      }
    }
}

static gboolean
afsocket_sd_setup_socket(AFSocketSourceDriver *self, gint fd)
{
  return afsocket_setup_socket(fd, self->sock_options_ptr, AFSOCKET_DIR_RECV);
}

void
afsocket_sd_free_instance(AFSocketSourceDriver *self)
{
  g_sockaddr_unref(self->bind_addr);
  self->bind_addr = NULL;
  
  log_drv_free_instance(&self->super);
}

static void
afsocket_sd_free(LogPipe *s)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  
  afsocket_sd_free_instance(self);
  g_free(self);
}

void
afsocket_sd_init_instance(AFSocketSourceDriver *self, SocketOptions *sock_options, guint32 flags)
{
  log_drv_init_instance(&self->super);
  
  self->super.super.init = afsocket_sd_init;
  self->super.super.deinit = afsocket_sd_deinit;
  self->super.super.free_fn = afsocket_sd_free;
  self->super.super.queue = log_pipe_forward_msg;
  self->super.super.notify = afsocket_sd_notify;
  self->sock_options_ptr = sock_options;
  self->setup_socket = afsocket_sd_setup_socket;
  self->max_connections = 10;
  self->listen_backlog = 255;
  self->flags = flags;
  log_reader_options_defaults(&self->reader_options);
}

/* socket destinations */

void afsocket_dd_reconnect(AFSocketDestDriver *self);

static const gchar *
afsocket_dd_format_stats_name(AFSocketDestDriver *self)
{
  static gchar stats_name[64];
  gchar *driver_name = NULL;
  
  switch (self->dest_addr->sa.sa_family)
    {
    case AF_UNIX:
      driver_name = !!(self->flags & AFSOCKET_STREAM) ? "unix-stream" : "unix-dgram";
      break;
    case AF_INET:
      driver_name = !!(self->flags & AFSOCKET_STREAM) ? "tcp" : "udp";
      break;
#if ENABLE_IPV6
    case AF_INET6:
      driver_name = !!(self->flags & AFSOCKET_STREAM) ? "tcp6" : "udp6";
      break;    
#endif
    }
  
  g_snprintf(stats_name, sizeof(stats_name), "%s(%s)", 
             driver_name,
             self->dest_name);
  
  return stats_name;
}

static gboolean
afsocket_dd_reconnect_timer(gpointer s)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;
  
  afsocket_dd_reconnect(self);
  return FALSE;
}


static void
afsocket_dd_start_reconnect_timer(AFSocketDestDriver *self)
{
  if (self->reconnect_timer)
    g_source_remove(self->reconnect_timer);
  self->reconnect_timer = g_timeout_add(self->time_reopen * 1000, afsocket_dd_reconnect_timer, self);
}

gboolean
afsocket_dd_connected(AFSocketDestDriver *self)
{
  gchar buf1[256], buf2[256];
  int error = 0;
  socklen_t errorlen = sizeof(error);
  FDWrite *write;
  
  if (self->flags & AFSOCKET_STREAM)
    {
      if (getsockopt(self->fd, SOL_SOCKET, SO_ERROR, &error, &errorlen) == -1)
        {
          msg_error("getsockopt(SOL_SOCKET, SO_ERROR) failed for connecting socket",
                    evt_tag_errno(EVT_TAG_OSERROR, errno),
                    evt_tag_int("reconnect", self->time_reopen),
                    NULL);
          close(self->fd);
          goto error_reconnect;
        }
      if (error)
        {
          msg_error("Connection failed",
                    evt_tag_str("server", g_sockaddr_format(self->dest_addr, buf2, sizeof(buf2))),
                    evt_tag_errno(EVT_TAG_OSERROR, error),
                    evt_tag_int("time_reopen", self->time_reopen),
                    NULL);
          close(self->fd);
          goto error_reconnect;
        }
    }
  msg_verbose("Syslog connection established",
              evt_tag_str("from", g_sockaddr_format(self->bind_addr, buf1, sizeof(buf1))),
              evt_tag_str("to", g_sockaddr_format(self->dest_addr, buf2, sizeof(buf2))),
              NULL);

  if (self->source_id)
    {
      g_source_remove(self->source_id);
      self->source_id = 0;
    }
    
  write = fd_write_new(self->fd);
  
  log_writer_reopen(self->writer, write);
  return TRUE;
 error_reconnect:
  afsocket_dd_start_reconnect_timer(self);
  return FALSE;
}

gboolean
afsocket_dd_start_connect(AFSocketDestDriver *self)
{
  int sock, rc;
  

  if (!afsocket_open_socket(self->bind_addr, !!(self->flags & AFSOCKET_STREAM), &sock))
    {
      return FALSE;
    }
    
  if (self->setup_socket && !self->setup_socket(self, sock))
    {
      close(sock);
      return FALSE;
    }
  
  rc = g_connect(sock, self->dest_addr);
  if (rc == G_IO_STATUS_NORMAL)
    {
      self->fd = sock;
      afsocket_dd_connected(self);
    }
  else if (rc == G_IO_STATUS_ERROR && errno == EINPROGRESS)
    {
      GSource *source;

      /* we must wait until connect succeeds */

      self->fd = sock;
      source = g_connect_source_new(sock);
      
      /* a reference is added on behalf of the source, it will be unrefed when
       * the source is destroyed */
      log_pipe_ref(&self->super.super);
      g_source_set_callback(source, (GSourceFunc) afsocket_dd_connected, self, (GDestroyNotify) log_pipe_unref);
      self->source_id = g_source_attach(source, NULL);
      g_source_unref(source);
    }
  else 
    {
      /* error establishing connection */
      msg_error("Connection failed",
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      close(sock);
      return FALSE;
    }

  return TRUE;
}

void
afsocket_dd_reconnect(AFSocketDestDriver *self)
{
  if (!afsocket_dd_start_connect(self))
    {
      msg_error("Initiating connection failed, reconnecting",
                evt_tag_int("time_reopen", self->time_reopen),
                NULL);
      afsocket_dd_start_reconnect_timer(self);
    }
}

gboolean
afsocket_dd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;
  
  if (cfg)
    {
      self->time_reopen = cfg->time_reopen;
    }
  if (!self->writer)
    {
      log_writer_options_init(&self->writer_options, cfg, 0, afsocket_dd_format_stats_name(self));
      /* NOTE: we open our writer with no fd, so we can send messages down there
       * even while the connection is not established */
  
      self->writer = log_writer_new(LW_FORMAT_PROTO | (self->flags & AFSOCKET_STREAM ? LW_DETECT_EOF : 0), &self->super.super, &self->writer_options);
      log_pipe_init(self->writer, NULL, NULL);
      log_pipe_append(&self->super.super, self->writer);
    }
    
  afsocket_dd_reconnect(self);
  return TRUE;
}

gboolean
afsocket_dd_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  if (self->reconnect_timer)
    g_source_remove(self->reconnect_timer);
  if (self->source_id && g_source_remove(self->source_id))
    {
      msg_verbose("Closing connecting fd",
                  evt_tag_int("fd", self->fd),
                  NULL);
      close(self->fd);
    }
  if (self->writer)
    log_pipe_deinit(self->writer, NULL, NULL);
  return TRUE;
}

static void
afsocket_dd_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;
  switch (notify_code)
    {
    case NC_CLOSE:
    case NC_WRITE_ERROR:
      msg_error("Connection broken",
                evt_tag_int("time_reopen", self->time_reopen),
                NULL);
      if (self->reconnect_timer)
        {
          g_source_remove(self->reconnect_timer);
          self->reconnect_timer = 0;
        }
      self->reconnect_timer = g_timeout_add(self->time_reopen * 1000, afsocket_dd_reconnect_timer, self);
      break;
    }
}

static gboolean
afsocket_dd_setup_socket(AFSocketDestDriver *self, gint fd)
{
  return afsocket_setup_socket(fd, self->sock_options_ptr, AFSOCKET_DIR_SEND);
}

void
afsocket_dd_free(LogPipe *s)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  g_sockaddr_unref(self->bind_addr);
  g_sockaddr_unref(self->dest_addr);
  log_pipe_unref(self->writer);
  g_free(self->hostname);
  log_writer_options_destroy(&self->writer_options);
  log_drv_free_instance(&self->super);
  g_free(self->dest_name);
  g_free(s);
}

void 
afsocket_dd_init_instance(AFSocketDestDriver *self, SocketOptions *sock_options, guint32 flags, gchar *dest_name)
{
  log_drv_init_instance(&self->super);
  
  log_writer_options_defaults(&self->writer_options);
  self->super.super.init = afsocket_dd_init;
  self->super.super.deinit = afsocket_dd_deinit;
  self->super.super.queue = log_pipe_forward_msg;
  self->super.super.free_fn = afsocket_dd_free;
  self->super.super.notify = afsocket_dd_notify;
  self->setup_socket = afsocket_dd_setup_socket;
  self->sock_options_ptr = sock_options;
  self->flags = flags;
  self->dest_name = dest_name;
}
