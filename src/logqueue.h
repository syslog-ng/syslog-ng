/*
 * Copyright (C) 2002-2007 BalaBit IT Ltd.
 * Copyright (C) 2002-2007 Balazs Scheidler
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

typedef struct _LogQueue 
{
  GQueue *q;
  gint qsize;
} LogQueue;

static inline gint64 
log_queue_get_length(LogQueue *self)
{
  return g_queue_get_length(self->q) / 2;
}

gboolean log_queue_push_tail(LogQueue *self, LogMessage *msg, gint path_flags);
gboolean log_queue_pop_head(LogQueue *self, LogMessage **msg, gint *path_flags);
LogQueue *log_queue_new(gint qsize);
void log_queue_free(LogQueue *self);


#endif
