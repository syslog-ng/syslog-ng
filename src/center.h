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

#ifndef CENTER_H_INCLUDED
#define CENTER_H_INCLUDED

#include "syslog-ng.h"

#include "logpipe.h"

#define EP_SOURCE 1
#define EP_FILTER 2
#define EP_DESTINATION 3

typedef struct _LogEndpoint
{
  struct _LogEndpoint *ep_next;
  GString *name;
  gint type;
  gpointer ref;
} LogEndpoint;

LogEndpoint *log_endpoint_new(gint type, gchar *name);
void log_endpoint_append(LogEndpoint *a, LogEndpoint *b);
void log_endpoint_free(LogEndpoint *self);


#define LC_CATCHALL 1
#define LC_FALLBACK 2
#define LC_FINAL    4
#define LC_FLOW_CONTROL 8

typedef struct _LogConnection
{
  GHashTable *source_cache;
  GPtrArray *sources;
  GPtrArray *filters;
  GPtrArray *destinations;
  guint32 flags;
} LogConnection;

LogConnection *log_connection_new(LogEndpoint *endpoints, guint32 flags);
void log_connection_free(LogConnection *self);

#define LC_STATE_INIT_SOURCES 1
#define LC_STATE_INIT_DESTS 2
#define LC_STATE_WORKING 3

typedef struct _LogCenter 
{
  LogPipe super;
  GlobalConfig *cfg;
  PersistentConfig *persist;
  gint state;
  gboolean success;
  guint32 *received_messages;
  guint32 *queued_messages;
} LogCenter;

LogCenter *log_center_new(void);

static inline LogCenter *
log_center_ref(LogCenter *s)
{
  return (LogCenter *) log_pipe_ref((LogPipe *) s);
}

static inline void
log_center_unref(LogCenter *s)
{
  log_pipe_unref((LogPipe *) s);
}

#endif
