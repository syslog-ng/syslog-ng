/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#include "logmsg/logmsg.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster-key-builder.h"

typedef void (*LogQueuePushNotifyFunc)(gpointer user_data);

typedef struct _LogQueue LogQueue;

typedef const char *QueueType;

typedef struct _LogQueueMetrics
{
  struct
  {
    StatsClusterKey *output_events_sc_key;
    StatsClusterKey *memory_usage_sc_key;

    StatsCounterItem *queued_messages;
    StatsCounterItem *dropped_messages;
    StatsCounterItem *memory_usage;
  } shared;

  struct
  {
    StatsClusterKey *events_sc_key;
    StatsClusterKey *memory_usage_sc_key;

    StatsCounterItem *memory_usage;
    StatsCounterItem *queued_messages;
  } owned;
} LogQueueMetrics;

struct _LogQueue
{
  QueueType type;
  GAtomicCounter ref_cnt;

  gint throttle;
  gint throttle_buckets;
  struct timespec last_throttle_check;

  gchar *persist_name;

  LogQueueMetrics metrics;

  GMutex lock;
  LogQueuePushNotifyFunc parallel_push_notify;
  gpointer parallel_push_data;
  GDestroyNotify parallel_push_data_destroy;

  /* queue management */
  gboolean (*keep_on_reload)(LogQueue *self);
  gint64 (*get_length)(LogQueue *self);
  gboolean (*is_empty_racy)(LogQueue *self);
  void (*push_tail)(LogQueue *self, LogMessage *msg, const LogPathOptions *path_options);
  LogMessage *(*pop_head)(LogQueue *self, LogPathOptions *path_options);
  LogMessage *(*peek_head)(LogQueue *self);
  void (*ack_backlog)(LogQueue *self, gint n);
  void (*rewind_backlog)(LogQueue *self, guint rewind_count);
  void (*rewind_backlog_all)(LogQueue *self);

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

static inline gboolean
log_queue_is_empty_racy(LogQueue *self)
{
  if (self->is_empty_racy)
    return self->is_empty_racy(self);
  else
    return (self->get_length(self) == 0);
}

static inline void
log_queue_push_tail(LogQueue *self, LogMessage *msg, const LogPathOptions *path_options)
{
  self->push_tail(self, msg, path_options);
}

static inline LogMessage *
log_queue_pop_head(LogQueue *self, LogPathOptions *path_options)
{
  LogMessage *msg = NULL;

  if (self->throttle && self->throttle_buckets == 0)
    return NULL;

  msg = self->pop_head(self, path_options);

  if (msg && self->throttle_buckets > 0)
    self->throttle_buckets--;

  return msg;
}

static inline LogMessage *
log_queue_peek_head(LogQueue *self)
{
  return self->peek_head(self);
}

static inline LogMessage *
log_queue_pop_head_ignore_throttle(LogQueue *self, LogPathOptions *path_options)
{
  return self->pop_head(self, path_options);
}

static inline void
log_queue_rewind_backlog(LogQueue *self, guint rewind_count)
{
  self->rewind_backlog(self, rewind_count);
}

static inline void
log_queue_rewind_backlog_all(LogQueue *self)
{
  self->rewind_backlog_all(self);
}

static inline void
log_queue_ack_backlog(LogQueue *self, guint rewind_count)
{
  self->ack_backlog(self, rewind_count);
}

static inline LogQueue *
log_queue_ref(LogQueue *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt) > 0);

  if (self)
    {
      g_atomic_counter_inc(&self->ref_cnt);
    }
  return self;
}

static inline void
log_queue_unref(LogQueue *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt) > 0);

  if (self && g_atomic_counter_dec_and_test(&self->ref_cnt))
    {
      if (self->free_fn)
        self->free_fn(self);
    }
}

static inline void
log_queue_set_throttle(LogQueue *self, gint throttle)
{
  self->throttle = throttle;
  self->throttle_buckets = throttle;
}

static inline gboolean
log_queue_has_type(LogQueue *self, QueueType type)
{
  return g_strcmp0(self->type, type) == 0;
}

void log_queue_memory_usage_add(LogQueue *self, gsize value);
void log_queue_memory_usage_sub(LogQueue *self, gsize value);

void log_queue_queued_messages_add(LogQueue *self, gsize value);
void log_queue_queued_messages_sub(LogQueue *self, gsize value);
void log_queue_queued_messages_inc(LogQueue *self);
void log_queue_queued_messages_dec(LogQueue *self);
void log_queue_queued_messages_reset(LogQueue *self);

void log_queue_dropped_messages_inc(LogQueue *self);

void log_queue_push_notify(LogQueue *self);
void log_queue_reset_parallel_push(LogQueue *self);
void log_queue_set_parallel_push(LogQueue *self, LogQueuePushNotifyFunc parallel_push_notify, gpointer user_data,
                                 GDestroyNotify user_data_destroy);
gboolean log_queue_check_items(LogQueue *self, gint *timeout, LogQueuePushNotifyFunc parallel_push_notify,
                               gpointer user_data, GDestroyNotify user_data_destroy);
void log_queue_init_instance(LogQueue *self, const gchar *persist_name, gint stats_level,
                             StatsClusterKeyBuilder *driver_sck_builder,
                             StatsClusterKeyBuilder *queue_sck_builder);

void log_queue_free_method(LogQueue *self);

#endif
