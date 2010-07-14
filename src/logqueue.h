/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
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
  
#ifndef LOGQUEUE_H_INCLUDED
#define LOGQUEUE_H_INCLUDED

#include "logmsg.h"

typedef struct _LogQueue LogQueue;

gint64 log_queue_get_length(LogQueue *self);
gboolean log_queue_push_tail(LogQueue *self, LogMessage *msg, const LogPathOptions *path_options);
gboolean log_queue_push_head(LogQueue *self, LogMessage *msg, const LogPathOptions *path_options);
gboolean log_queue_pop_head(LogQueue *self, LogMessage **msg, LogPathOptions *path_flags, gboolean push_to_backlog);
void log_queue_rewind_backlog(LogQueue *self);
void log_queue_ack_backlog(LogQueue *self, gint n);

LogQueue *log_queue_new(gint qoverflow_size);
void log_queue_free(LogQueue *self);

#endif
