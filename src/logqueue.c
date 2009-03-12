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

#include "logqueue.h"
#include "logpipe.h"
#include "messages.h"
#include "serialize.h"
#include "stats.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


#define LOG_PATH_OPTIONS_TO_POINTER(lpo) GUINT_TO_POINTER(0x80000000 | (lpo)->flow_control)

/* NOTE: this must not evaluate ptr multiple times, otherwise the code that
 * uses this breaks, as it passes the result of a g_queue_pop_head call,
 * which has side effects.
 */
#define POINTER_TO_LOG_PATH_OPTIONS(ptr, lpo) (lpo)->flow_control = (GPOINTER_TO_INT(ptr) & ~0x80000000)

struct _LogQueue
{
  GQueue *qoverflow;   /* entries that did not fit to the disk based queue */
  GQueue *qbacklog;    /* entries that were sent but not acked yet */
  gint qoverflow_size; /* in number of elements */
  guint32 *stored_messages;
};

gint64 
log_queue_get_length(LogQueue *self)
{
  return (self->qoverflow->length / 2);
}

/**
 *
 * Returns: TRUE if the messages could be consumed, FALSE otherwise
 **/
gboolean
log_queue_push_tail(LogQueue *self, LogMessage *msg, const LogPathOptions *path_options)
{
  LogPathOptions local_options = *path_options;
  
  if ((self->qoverflow->length / 2) < self->qoverflow_size)
    {
      g_queue_push_tail(self->qoverflow, msg);
      g_queue_push_tail(self->qoverflow, LOG_PATH_OPTIONS_TO_POINTER(path_options));
      log_msg_ref(msg);
      local_options.flow_control = FALSE;
    }
  else
    {
      return FALSE;
    }
  stats_counter_inc(self->stored_messages);
  log_msg_ack(msg, &local_options);
  log_msg_unref(msg);
  return TRUE;
}

gboolean
log_queue_push_head(LogQueue *self, LogMessage *msg, const LogPathOptions *path_options)
{
  g_queue_push_head(self->qoverflow, LOG_PATH_OPTIONS_TO_POINTER(path_options));
  g_queue_push_head(self->qoverflow, msg);
  stats_counter_inc(self->stored_messages);
  return TRUE;
}

gboolean
log_queue_pop_head(LogQueue *self, LogMessage **msg, LogPathOptions *path_options, gboolean push_to_backlog)
{
  if (self->qoverflow->length > 0)
    {
      *msg = g_queue_pop_head(self->qoverflow);
      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(self->qoverflow), path_options);
    }
  else
    {
      return FALSE;
    }
  stats_counter_dec(self->stored_messages);

  if (push_to_backlog)
    {
      log_msg_ref(*msg);
      g_queue_push_tail(self->qbacklog, *msg);
      g_queue_push_tail(self->qbacklog, LOG_PATH_OPTIONS_TO_POINTER(path_options));
    }    
  return TRUE;
}

void
log_queue_ack_backlog(LogQueue *self, gint n)
{
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  gint i;
  
  for (i = 0; i < n; i++)
    {
      msg = g_queue_pop_head(self->qbacklog);
      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(self->qbacklog), &path_options);
      log_msg_ack(msg, &path_options);
      log_msg_unref(msg);
    }
}


/* Move items on our backlog back to our qoverflow queue. Please note that this
 * function does not really care about qoverflow size, it has to put the backlog
 * somewhere. The backlog is emptied as that will be filled if we send the
 * items again.
 *
 */
void
log_queue_rewind_backlog(LogQueue *self)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg;
  gint i, n = self->qbacklog->length / 2;

  for (i = 0; i < n; i++)
    {
      msg = g_queue_pop_head(self->qbacklog);
      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(self->qbacklog), &path_options);
      
      /* NOTE: reverse order as we are pushing to the head */
      g_queue_push_head(self->qoverflow, LOG_PATH_OPTIONS_TO_POINTER(&path_options));
      g_queue_push_head(self->qoverflow, msg);
    }
}

LogQueue *
log_queue_new(gint qoverflow_size)
{
  LogQueue *self = g_new0(LogQueue, 1);
  
  self->qoverflow = g_queue_new();
  self->qbacklog = g_queue_new();
  self->qoverflow_size = qoverflow_size;
  return self;
}

static void
log_queue_free_queue(GQueue *q)
{
  while (!g_queue_is_empty(q))
    {
      LogMessage *lm;
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
      
      lm = g_queue_pop_head(q);
      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(q), &path_options);
      log_msg_ack(lm, &path_options);
      log_msg_unref(lm);
    }
  g_queue_free(q);
}

void
log_queue_free(LogQueue *self)
{
  log_queue_free_queue(self->qoverflow);
  log_queue_free_queue(self->qbacklog);
  g_free(self);
}


