/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
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
