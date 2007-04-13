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

static gboolean
afsocket_open_socket(GSockAddr *bind_addr, int stream_or_dgram, int *fd)
{
  gint sock;
  
  if (stream_or_dgram)
    sock = socket(bind_addr->sa.sa_family, SOCK_STREAM, 0);
  else
    sock = socket(bind_addr->sa.sa_family, SOCK_DGRAM, 0);
    
  g_fd_set_nonblock(sock, TRUE);
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
    return FALSE;
}

static gboolean
afsocket_sc_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;

  self->reader = log_reader_new(fd_read_new(self->sock, (self->owner->flags & AFSOCKET_DGRAM) ? FR_RECV : 0), 
                                (self->owner->flags & AFSOCKET_LOCAL) ? LR_LOCAL : 0 | 
                                (self->owner->flags & AFSOCKET_DGRAM) ? LR_PKTTERM : 0 |
                                (self->owner->flags & AFSOCKET_PROTO_RFC3164) ? LR_STRICT : 0, 
                                s, &self->owner->reader_options);
  log_pipe_append(self->reader, s);
  log_pipe_init(self->reader, NULL, NULL);
  return TRUE;
}

static gboolean
afsocket_sc_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;
  
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
      else
        msg_notice("Internal error, message without source address;", NULL);
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
        afsocket_sd_close_connection(self->owner, self);
        break;
      }
    }
}

static void 
afsocket_sc_free(LogPipe *s)
{
  AFSocketSourceConnection *self = (AFSocketSourceConnection *) s;

  self->owner->connections = g_list_remove(self->owner->connections, self);
  log_pipe_unref(&self->owner->super.super);
  
  g_assert(!self->reader);
  g_sockaddr_unref(self->peer_addr);
  g_free(self);
}

AFSocketSourceConnection *
afsocket_sc_new(struct _AFSocketSourceDriver *owner, GSockAddr *peer_addr, int fd)
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
afsocket_sd_set_listener_keep_alive(LogDriver *s, gint enable)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  
  if (enable)
    self->flags |= AFSOCKET_LISTENER_KEEP_ALIVE;
  else
    self->flags &= ~AFSOCKET_LISTENER_KEEP_ALIVE;
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
      
      conn = afsocket_sc_new(self, peer_addr, fd);
      log_pipe_init(&conn->super, NULL, NULL);
      log_pipe_append(&conn->super, &self->super.super);
    }
  return TRUE;
}

static gboolean
afsocket_sd_accept(gpointer s)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  GSockAddr *peer_addr;
  gchar buf1[256], buf2[256];
  gint new_fd;
  gboolean res;
  
  if (g_accept(self->fd, &new_fd, &peer_addr) != G_IO_STATUS_NORMAL)
    {
      msg_error("Error accepting new connection",
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return TRUE;
    }
  g_fd_set_nonblock(new_fd, TRUE);
    
  msg_verbose("Syslog connection accepted",
              evt_tag_str("from", g_sockaddr_format(peer_addr, buf1, sizeof(buf1))),
              evt_tag_str("to", g_sockaddr_format(self->bind_addr, buf2, sizeof(buf2))),
              NULL);

  res = afsocket_sd_process_connection(self, peer_addr, new_fd);
  g_sockaddr_unref(peer_addr);
  return res;

}

static void
afsocket_sd_close_connection(AFSocketSourceDriver *self, AFSocketSourceConnection *sc)
{
  log_pipe_deinit(&sc->super, NULL, NULL);
  log_pipe_unref(&sc->super);
}

static void
afsocket_sd_kill_connection(AFSocketSourceConnection *sc)
{
  log_pipe_deinit(&sc->super, NULL, NULL);
  log_pipe_unref(&sc->super);
}

gboolean
afsocket_sd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFSocketSourceDriver *self = (AFSocketSourceDriver *) s;
  gint sock;
  gboolean res = FALSE;
  
  log_reader_options_init(&self->reader_options, cfg);
  
  /* fetch persistent connections first */  
  if ((self->flags & AFSOCKET_KEEP_ALIVE))
    {
      GList *p;

      self->connections = persist_config_fetch(persist, afsocket_sd_format_persist_name(self, FALSE));
      for (p = self->connections; p; p = p->next)
        {
          log_pipe_append((LogPipe *) p->data, s);
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

      if (sock == -1 && !afsocket_open_socket(self->bind_addr, !!(self->flags & AFSOCKET_STREAM), &sock))
        {
          return FALSE;
        }

      
      /* set up listening source */
      self->fd = sock;
      listen(sock, self->listen_backlog);
      
      source = g_listen_source_new(self->fd);
      
      /* the listen_source references us, which is freed when the source is deleted */
      log_pipe_ref(s); 
      g_source_set_callback(source, afsocket_sd_accept, self, (GDestroyNotify) log_pipe_unref);
      self->source_id = g_source_attach(source, NULL);
      g_source_unref(source);
    }
  else
    {
      if (!self->connections && !afsocket_open_socket(self->bind_addr, !!(self->flags & AFSOCKET_STREAM), &sock))
        {
          /* we could not recover persistent listener and could not open one either */
          return FALSE;
        }
      self->fd = -1;
      
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
          g_list_free_1(p);
        }
    }
  else
    {
      /* for AFSOCKET_STREAM source drivers this is a list, for
       * AFSOCKET_DGRAM this is a single connection */
      
      persist_config_add(persist, afsocket_sd_format_persist_name(self, FALSE), self->connections, (GDestroyNotify) afsocket_sd_kill_connection);
    }
  self->connections = NULL;


  if (self->flags & AFSOCKET_STREAM)
    {
      
      g_source_remove(self->source_id);
      self->source_id = 0;
      if ((self->flags & AFSOCKET_LISTENER_KEEP_ALIVE) == 0)
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
        g_assert(0);
        log_pipe_deinit(sender, NULL, NULL);
        log_pipe_unref(sender);
        break;
      }
    }
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
afsocket_sd_init_instance(AFSocketSourceDriver *self, guint32 flags)
{
  log_drv_init_instance(&self->super);
  
  self->super.super.init = afsocket_sd_init;
  self->super.super.deinit = afsocket_sd_deinit;
  self->super.super.free_fn = afsocket_sd_free;
  self->super.super.queue = log_pipe_forward_msg;
  self->super.super.notify = afsocket_sd_notify;
  self->max_connections = 10;
  self->listen_backlog = 255;
  self->flags = flags;
  log_reader_options_defaults(&self->reader_options);
}

/* socket destinations */

gboolean afsocket_dd_reconnect(AFSocketDestDriver *self);

static gboolean
afsocket_dd_reconnect_timer(gpointer s)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;
  
  afsocket_dd_reconnect(self);
  return FALSE;
}

gboolean
afsocket_dd_connected(AFSocketDestDriver *self)
{
  gchar buf1[256], buf2[256];
  int error = 0;
  socklen_t errorlen = sizeof(error);
  
  if (getsockopt(self->fd, SOL_SOCKET, SO_ERROR, &error, &errorlen) == -1)
    {
      msg_error("getsockopt(SOL_SOCKET, SO_ERROR) failed for connecting socket",
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                evt_tag_int("reconnect", self->time_reopen),
                NULL);
      close(self->fd);

      self->reconnect_timer = g_timeout_add(self->time_reopen * 1000, afsocket_dd_reconnect_timer, self);
      return FALSE;
    }
  if (error)
    {
      msg_error("Connection failed",
                evt_tag_errno(EVT_TAG_OSERROR, error),
                evt_tag_int("reconnect", self->time_reopen),
                NULL);
      close(self->fd);

      self->reconnect_timer = g_timeout_add(self->time_reopen * 1000, afsocket_dd_reconnect_timer, self);
      return FALSE;
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
    
  log_writer_reopen(self->writer, fd_write_new(self->fd));
  if (!log_pipe_init(self->writer, NULL, NULL))
    return FALSE;
  return TRUE;
}

gboolean
afsocket_dd_reconnect(AFSocketDestDriver *self)
{
  int sock, rc;
  
  if (!afsocket_open_socket(self->bind_addr, !!(self->flags & AFSOCKET_STREAM), &sock))
    {
      return FALSE;
    }
  
  rc = g_connect(sock, self->dest_addr);
  if (rc == G_IO_STATUS_NORMAL)
    {
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
                evt_tag_int("reconnect", self->time_reopen),
                NULL);
      close(sock);
      
      self->reconnect_timer = g_timeout_add(self->time_reopen * 1000, afsocket_dd_reconnect_timer, self);
      return FALSE;
    }

  return TRUE;
}

gboolean
afsocket_dd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;
  
  if (cfg)
    {
      self->time_reopen = cfg->time_reopen;
    }

  log_writer_options_init(&self->writer_options, cfg, !!(self->flags & AFSOCKET_PROTO_RFC3164));
  /* NOTE: we open our writer with no fd, so we can send messages down there
   * even while the connection is not established */
         
  self->writer = log_writer_new(LW_FORMAT_PROTO | LW_DETECT_EOF, &self->super.super, &self->writer_options);
  log_pipe_append(&self->super.super, self->writer);
  if (!afsocket_dd_reconnect(self))
    {
      return FALSE;
    }
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
      if (self->reconnect_timer)
        {
          g_source_remove(self->reconnect_timer);
          self->reconnect_timer = 0;
        }
      afsocket_dd_reconnect(self);
      break;
    }
}

void
afsocket_dd_free(LogPipe *s)
{
  AFSocketDestDriver *self = (AFSocketDestDriver *) s;

  g_sockaddr_unref(self->bind_addr);
  g_sockaddr_unref(self->dest_addr);
  log_drv_free_instance(&self->super);
  g_free(s);
}

void 
afsocket_dd_init_instance(AFSocketDestDriver *self, guint32 flags)
{
  log_drv_init_instance(&self->super);
  
  log_writer_options_defaults(&self->writer_options);
  self->super.super.init = afsocket_dd_init;
  self->super.super.deinit = afsocket_dd_deinit;
  self->super.super.queue = log_pipe_forward_msg;
  self->super.super.free_fn = afsocket_dd_free;
  self->super.super.notify = afsocket_dd_notify;
  self->flags = flags;
}
