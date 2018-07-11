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

#ifndef LOGQUEUE_REDIS_H_INCLUDED
#define LOGQUEUE_REDIS_H_INCLUDED

#include <hiredis/hiredis.h>

#include "redisq-options.h"
#include "logmsg/logmsg.h"
#include "logqueue.h"

#define LOG_PATH_OPTIONS_TO_POINTER(lpo) GUINT_TO_POINTER(0x80000000 | (lpo)->ack_needed)

/* NOTE: this must not evaluate ptr multiple times, otherwise the code that
 * uses this breaks, as it passes the result of a g_queue_pop_head call,
 * which has side effects.
 */
#define POINTER_TO_LOG_PATH_OPTIONS(ptr, lpo) (lpo)->ack_needed = (GPOINTER_TO_INT(ptr) & ~0x80000000)

typedef struct _LogQueueRedis LogQueueRedis;

struct _LogQueueRedis
{
  LogQueue super;

  GQueue *qbacklog;

  GMutex *redis_thread_mutex;
  redisContext *c;
  RedisQueueOptions *redis_options;
  gchar *persist_name;

  LogMessage *(*read_message)(LogQueueRedis *self, LogPathOptions *path_options);
  gboolean (*write_message)(LogQueueRedis *self, LogMessage *msg, const LogPathOptions *path_options);
  gboolean (*delete_message)(LogQueueRedis *self);
};

extern QueueType log_queue_redis_type;

LogQueue *log_queue_redis_init_instance(LogQueueRedis *self, const gchar *persist_name);
LogQueueRedis *redis_server_init(RedisQueueOptions *options, const gchar *name);
void redis_server_free(LogQueueRedis *self);

#endif
