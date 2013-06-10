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
#include "stats.h"

extern gint log_queue_max_threads;

typedef void (*LogQueuePushNotifyFunc)(gpointer user_data);

typedef struct _LogQueue LogQueue;

struct _LogQueue
{
  /* this object is reference counted, but it is _not_ thread safe to
     acquire/release references in code executing in parallel */
  gint ref_cnt;
  gint throttle;
  gint throttle_buckets;
  GTimeVal last_throttle_check;

  gchar *persist_name;
  StatsCounterItem *stored_messages;
  StatsCounterItem *dropped_messages;

  GStaticMutex lock;
  gint parallel_push_notify_limit;
  LogQueuePushNotifyFunc parallel_push_notify;
  gpointer parallel_push_data;
  GDestroyNotify parallel_push_data_destroy;

  /* queue management */
  gboolean (*keep_on_reload)(LogQueue *self);
  gint64 (*get_length)(LogQueue *self);
  void (*push_tail)(LogQueue *self, LogMessage *msg, const LogPathOptions *path_options);
  void (*push_head)(LogQueue *self, LogMessage *msg, const LogPathOptions *path_options);
  gboolean (*pop_head)(LogQueue *self, LogMessage **msg, LogPathOptions *path_options, gboolean push_to_backlog, gboolean ignore_throttle);
  void (*ack_backlog)(LogQueue *self, gint n);
  void (*rewind_backlog)(LogQueue *self);

  void (*free_fn)(LogQueue *self);
};

static inline gboolean
log_queue_keep_on_reload(LogQueue *self)
{
  if (self->keep_on_reload)
    return self->keep_on_reload(self);
  return TRUE;
}

static inline gint64
log_queue_get_length(LogQueue *self)
{
  return self->get_length(self);
}

static inline void
log_queue_push_tail(LogQueue *self, LogMessage *msg, const LogPathOptions *path_options)
{
  self->push_tail(self, msg, path_options);
}

static inline void
log_queue_push_head(LogQueue *self, LogMessage *msg, const LogPathOptions *path_options)
{
  self->push_head(self, msg, path_options);
}

static inline gboolean
log_queue_pop_head(LogQueue *self, LogMessage **msg, LogPathOptions *path_options, gboolean push_to_backlog, gboolean ignore_throttle)
{
  return self->pop_head(self, msg, path_options, push_to_backlog, ignore_throttle);
}

static inline void
log_queue_rewind_backlog(LogQueue *self)
{
  return self->rewind_backlog(self);
}

static inline void
log_queue_ack_backlog(LogQueue *self, gint n)
{
  return self->ack_backlog(self, n);
}

static inline LogQueue *
log_queue_ref(LogQueue *self)
{
  self->ref_cnt++;
  return self;
}

static inline void
log_queue_unref(LogQueue *self)
{
  if (--self->ref_cnt == 0)
    self->free_fn(self);
}

static inline void
log_queue_set_throttle(LogQueue *self, gint throttle)
{
  self->throttle = throttle;
  self->throttle_buckets = throttle;
}

/*
 * This assertion marks the assumption that a given function is
 * running solely from the output thread. It _will not_ catch all
 * invalid invocations, but will most probably catch it sooner rather
 * than later.
 *
 * The check it performs (parallel_push_notify is NULL) means that
 * there's no current parallel_push_notify callbacks pending.
 *
 * Since parallel_push_notify is set to non-NULL by the output thread
 * when waiting for elements to arrive (e.g. when going to sleep), it
 * is quite certain that if we are NOT in the output thread, that will
 * be set.
 */
#define log_queue_assert_output_thread(self)  g_assert(self->parallel_push_notify == NULL)

void log_queue_push_notify(LogQueue *self);
void log_queue_reset_parallel_push(LogQueue *self);
void log_queue_set_parallel_push(LogQueue *self, gint notify_limit, LogQueuePushNotifyFunc parallel_push_notify, gpointer user_data, GDestroyNotify user_data_destroy);
gboolean log_queue_check_items(LogQueue *self, gint batch_items, gboolean *partial_batch, gint *timeout, LogQueuePushNotifyFunc parallel_push_notify, gpointer user_data, GDestroyNotify user_data_destroy);
void log_queue_set_counters(LogQueue *self, StatsCounterItem *stored_messages, StatsCounterItem *dropped_messages);
void log_queue_init_instance(LogQueue *self, const gchar *persist_name);
void log_queue_free_method(LogQueue *self);

void log_queue_set_max_threads(gint max_threads);

#endif
