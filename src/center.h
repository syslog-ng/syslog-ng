/*
 * Copyright (c) 2002-2008 BalaBit IT Ltd, Budapest, Hungary
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

#define EP_SOURCE      1
#define EP_FILTER      2
#define EP_PARSER      3
#define EP_DESTINATION 4
#define EP_PIPE        5
#define EP_REWRITE      6

typedef struct _LogPipeItem LogPipeItem;

LogPipeItem *log_pipe_item_new(gint type, gchar *name);
LogPipeItem *log_pipe_item_new_ref(gint type, gpointer ref);
void log_pipe_item_append(LogPipeItem *a, LogPipeItem *b);
LogPipeItem *log_pipe_item_append_tail(LogPipeItem *a, LogPipeItem *b);
void log_pipe_item_free(LogPipeItem *self);


#define LC_CATCHALL 1
#define LC_FALLBACK 2
#define LC_FINAL    4
#define LC_FLOW_CONTROL 8

/**
 * This structure is used to hold the log statements as specified by the
 * user. Endpoints contain references to source/filter/parser/destination,
 * during initialization this is turned into a message processing pipeline.
 **/
typedef struct _LogConnection
{
  LogPipeItem *endpoints;
  guint32 flags;
} LogConnection;

LogConnection *log_connection_new(LogPipeItem *endpoints, guint32 flags);
void log_connection_free(LogConnection *self);
gint log_connection_lookup_flag(const gchar *flag);

#define LC_STATE_INIT_SOURCES 1
#define LC_STATE_INIT_DESTS 2
#define LC_STATE_WORKING 3

/**
 * LogCenter used to be the central processing element of syslog-ng, now it
 * is degraded to construct the message processing pipelines the user
 * specifies in its configuration file using log statements. There's no
 * coherent reason for its existence, apart from the fact that it'd bloat
 * the cfg.c file.
 **/
typedef struct _LogCenter 
{
  GPtrArray *initialized_pipes;
  gint state;
  guint32 *received_messages;
  guint32 *queued_messages;
} LogCenter;

gboolean log_center_init(LogCenter *self, GlobalConfig *cfg);
gboolean log_center_deinit(LogCenter *self);

LogCenter *log_center_new(void);
void log_center_free(LogCenter *s);

#endif
