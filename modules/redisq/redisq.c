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

#include "driver.h"
#include "messages.h"

#include "redisq.h"
#include "logqueue-redis.h"
#include "persist-state.h"

#define REDISQ_PLUGIN_NAME "redisq"

static LogQueue *
_acquire_queue(LogDestDriver *dd, const gchar *persist_name)
{
  RedisQDestPlugin *self = log_driver_get_plugin(&dd->super, RedisQDestPlugin, REDISQ_PLUGIN_NAME);
  GlobalConfig *cfg = log_pipe_get_config(&dd->super.super);
  LogQueue *queue = NULL;

  msg_debug("redisq: acquire queue");

  if (persist_name)
    queue = cfg_persist_config_fetch(cfg, persist_name);

  if (queue)
    {
      log_queue_unref(queue);
      queue = NULL;
    }

  queue = log_queue_redis_init_instance(cfg, persist_name);
  log_queue_set_throttle(queue, dd->throttle);

  return queue;
}

void
_release_queue(LogDestDriver *dd, LogQueue *queue)
{
  GlobalConfig *cfg = log_pipe_get_config(&dd->super.super);

  msg_debug("redisq: release queue");

  if (queue->persist_name)
    {
      cfg_persist_config_add(cfg, queue->persist_name, queue, (GDestroyNotify) log_queue_unref, FALSE);
    }
  else
    {
      log_queue_unref(queue);
    }
}

static gboolean
_attach(LogDriverPlugin *s, LogDriver *d)
{
  RedisQDestPlugin *self = (RedisQDestPlugin *) s;
  LogDestDriver *dd = (LogDestDriver *) d;
  GlobalConfig *cfg = log_pipe_get_config(&d->super);

  msg_debug("redisq: plugin attach");

  dd->acquire_queue = _acquire_queue;
  dd->release_queue = _release_queue;
  return TRUE;
}

static void
_free(LogDriverPlugin *s)
{
  RedisQDestPlugin *self = (RedisQDestPlugin *)s;
  msg_debug("redisq: plugin free");

  redis_queue_options_destroy(&self->options);
}


RedisQDestPlugin *
redisq_dest_plugin_new(void)
{
  RedisQDestPlugin *self = g_new0(RedisQDestPlugin, 1);

  msg_debug("redisq: plugin init");

  log_driver_plugin_init_instance(&self->super, REDISQ_PLUGIN_NAME);
  redis_queue_options_set_default_options(&self->options);
  self->super.attach = _attach;
  self->super.free_fn = _free;
  return self;
}
