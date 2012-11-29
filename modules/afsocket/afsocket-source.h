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

#ifndef AFSOCKET_SOURCE_H_INCLUDED
#define AFSOCKET_SOURCE_H_INCLUDED

#include "afsocket.h"
#include "driver.h"
#include "logreader.h"
#if BUILD_WITH_SSL
#include "tlscontext.h"
#endif

#include <iv.h>

#define AFSOCKET_WNDSIZE_INITED      0x10000

typedef struct _AFSocketSourceDriver AFSocketSourceDriver;

struct _AFSocketSourceDriver
{
  LogSrcDriver super;
  guint32 recvd_messages_are_local:1,
    syslog_protocol:1,
    connections_kept_alive_accross_reloads:1,
    require_tls:1,
    window_size_initialized:1;
  struct iv_fd listen_fd;
  gint fd;
  /* SOCK_DGRAM or SOCK_STREAM or other SOCK_XXX values used by the socket() call */
  gint sock_type;
  /* protocol parameter for the socket() call, 0 for default or IPPROTO_XXX for specific transports */
  gint sock_protocol;
  LogReaderOptions reader_options;
#if BUILD_WITH_SSL
  TLSContext *tls_context;
#endif
  gint address_family;
  GSockAddr *bind_addr;
  gchar *transport;
  gchar *logproto_name;
  gint max_connections;
  gint num_connections;
  gint listen_backlog;
  GList *connections;
  SocketOptions *sock_options_ptr;


  /*
   * Apply transport options, set up bind_addr based on the
   * information processed during parse time. This used to be
   * constructed during the parser, however that made the ordering of
   * various options matter and behave incorrectly when the port() was
   * specified _after_ transport(). Now, it collects the information,
   * and then applies them with a separate call to apply_transport()
   * during init().
   */

  gboolean (*apply_transport)(AFSocketSourceDriver *s);

  /* optionally acquire a socket from the runtime environment (e.g. systemd) */
  gboolean (*acquire_socket)(AFSocketSourceDriver *s, gint *fd);

  /* once the socket is opened, set up socket related options (IP_TTL,
     IP_TOS, SO_RCVBUF etc) */

  gboolean (*setup_socket)(AFSocketSourceDriver *s, gint fd);
};

void afsocket_sd_set_transport(LogDriver *s, const gchar *transport);
void afsocket_sd_set_keep_alive(LogDriver *self, gint enable);
void afsocket_sd_set_max_connections(LogDriver *self, gint max_connections);
#if BUILD_WITH_SSL
void afsocket_sd_set_tls_context(LogDriver *s, TLSContext *tls_context);
#else
#define afsocket_sd_set_tls_context(s, t)
#endif

static inline gboolean
afsocket_sd_acquire_socket(AFSocketSourceDriver *s, gint *fd)
{
  if (s->acquire_socket)
    return s->acquire_socket(s, fd);
  *fd = -1;
  return TRUE;
}

static inline gboolean
afsocket_sd_apply_transport(AFSocketSourceDriver *s)
{
  return s->apply_transport(s);
}

gboolean afsocket_sd_init(LogPipe *s);
gboolean afsocket_sd_deinit(LogPipe *s);

void afsocket_sd_init_instance(AFSocketSourceDriver *self, SocketOptions *sock_options, gint family, gint sock_type);
void afsocket_sd_free(LogPipe *self);

#endif
