/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 Mehul Prajapati <mehulprajapati2802@gmail.com>
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

#include "logqueue-redis.h"
#include "logpipe.h"
#include "messages.h"
#include "stats/stats-registry.h"
#include "reloc.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define ITEMS_PER_MESSAGE 2

QueueType log_queue_redis_type = "FIFO";

static gint64
_get_length(LogQueue *s)
{
  LogQueueRedis *self = (LogQueueRedis *) s;
  return (self->qredis->length) / ITEMS_PER_MESSAGE;
}

static void
_empty_queue(GQueue *q)
{
  while (q && (q->length) > 0)
    {
      LogMessage *msg;
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

      msg = g_queue_pop_head(q);
      POINTER_TO_LOG_PATH_OPTIONS(g_queue_pop_head(q), &path_options);

      log_msg_drop(msg, &path_options, AT_PROCESSED);
    }
}

static void
_push_tail(LogQueue *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogQueueRedis *self = (LogQueueRedis *) s;

  msg_debug("redisq: Pushing msg to tail");

  g_queue_push_tail (self->qredis, msg);
  g_queue_push_tail (self->qredis, LOG_PATH_OPTIONS_TO_POINTER(path_options));

  g_static_mutex_lock(&self->super.lock);
  log_queue_push_notify (&self->super);
  log_msg_ref (msg);
  g_static_mutex_unlock(&self->super.lock);

  stats_counter_inc(self->super.queued_messages);
  stats_counter_add(self->super.memory_usage, log_msg_get_size(msg));
}

static LogMessage *
_pop_head(LogQueue *s, LogPathOptions *path_options)
{
  LogQueueRedis *self = (LogQueueRedis *) s;
  LogMessage *msg = NULL;

  msg_debug("redisq: Pop msg from head");

  if (self->qredis->length > 0)
    {
      msg = g_queue_pop_head (self->qredis);
      POINTER_TO_LOG_PATH_OPTIONS (g_queue_pop_head(self->qredis), path_options);
    }

  if (msg != NULL)
    {
      stats_counter_dec(self->super.queued_messages);
      stats_counter_sub(self->super.memory_usage, log_msg_get_size(msg));

      log_msg_unref(msg);

      if (self->super.use_backlog)
        {
          msg_debug("redisq: qbacklog push");
          log_msg_ref (msg);
          g_queue_push_tail (self->qbacklog, msg);
          g_queue_push_tail (self->qbacklog, LOG_PATH_OPTIONS_TO_POINTER (path_options));
        }
    }

  return msg;
}

static void
_ack_backlog(LogQueue *s, gint num_msg_to_ack)
{
  LogQueueRedis *self = (LogQueueRedis *) s;
  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  guint i;

  msg_debug("redisq: ack backlog");

  for (i = 0; i < num_msg_to_ack; i++)
    {
      if (self->qbacklog->length < ITEMS_PER_MESSAGE)
        return;

      msg_debug("redisq: ack backlog loop");

      msg = g_queue_pop_head (self->qbacklog);
      POINTER_TO_LOG_PATH_OPTIONS (g_queue_pop_head (self->qbacklog), &path_options);
      log_msg_ack (msg, &path_options, AT_PROCESSED);
      log_msg_unref (msg);
    }
}

static void
_rewind_backlog(LogQueue *s, guint rewind_count)
{
  LogQueueRedis *self = (LogQueueRedis *) s;

  msg_debug("redisq: rewind backlog msg");
}

void
_backlog_all(LogQueue *s)
{
  LogQueueRedis *self = (LogQueueRedis *) s;

  msg_debug("redisq: backlog all");
}

static void
_free(LogQueue *s)
{
  LogQueueRedis *self = (LogQueueRedis *) s;

  msg_debug("redisq: free up queue");

  _empty_queue(self->qredis);
  g_queue_free(self->qredis);
  self->qredis = NULL;

  _empty_queue(self->qbacklog);
  g_queue_free(self->qbacklog);
  self->qbacklog = NULL;

  log_queue_free_method(&self->super);
}

static void
_set_virtual_functions(LogQueueRedis *self)
{
  self->super.type = log_queue_redis_type;
  self->super.get_length = _get_length;
  self->super.push_tail = _push_tail;
  self->super.pop_head = _pop_head;
  self->super.ack_backlog = _ack_backlog;
  self->super.rewind_backlog = _rewind_backlog;
  self->super.rewind_backlog_all = _backlog_all;
  self->super.free_fn = _free;
}

LogQueue *
log_queue_redis_init_instance(GlobalConfig *cfg, const gchar *persist_name)
{
  LogQueueRedis *self = g_new0(LogQueueRedis, 1);

  msg_debug("redisq: log queue init");

  log_queue_init_instance(&self->super, persist_name);
  self->qredis = g_queue_new();
  self->qbacklog = g_queue_new();
  self->cfg = cfg;
  _set_virtual_functions(self);
  return &self->super;
}
