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
#include "serialize.h"
#include "logmsg/logmsg-serialize.h"
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

#define ITEMS_PER_MESSAGE 1

QueueType log_queue_redis_type = "FIFO";

static gboolean
_send_redis_command(LogQueueRedis *self, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  redisReply *reply = redisvCommand(self->c, format, ap);
  va_end(ap);

  msg_debug("redisq: send redis command");

  gboolean retval = reply && (reply->type != REDIS_REPLY_ERROR);
  if (reply)
    freeReplyObject(reply);
  return retval;
}

static redisReply *
_get_redis_reply(LogQueueRedis *self, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  redisReply *reply = redisvCommand(self->c, format, ap);
  va_end(ap);

  msg_debug("redisq: get redis reply");

  gboolean retval = reply && (reply->type != REDIS_REPLY_ERROR);

  if (!retval)
    return NULL;

  return reply;
}

static gboolean
_check_connection_to_redis(LogQueueRedis *self)
{
  return _send_redis_command(self, "ping");
}

static gboolean
_authenticate_to_redis(LogQueueRedis *self, const gchar *password)
{
  return _send_redis_command(self, "AUTH %s", password);
}

static gboolean
_redis_dp_connect(LogQueueRedis *self, gboolean reconnect)
{
  redisReply *reply;

  msg_debug("redisq: Connecting to redis");

  if (reconnect && (self->c != NULL))
    {
      reply = redisCommand(self->c, "ping");

      if (reply)
        freeReplyObject(reply);

      if (!self->c->err)
        return TRUE;
      else
        self->c = redisConnect(self->redis_options->host, self->redis_options->port);
    }
  else
    self->c = redisConnect(self->redis_options->host, self->redis_options->port);

  if (self->c->err)
    {
      msg_error("redisq: REDIS server error, suspending",
                evt_tag_str("error", self->c->errstr));
      return FALSE;
    }

  if (self->redis_options->auth)
    if (!_authenticate_to_redis(self, self->redis_options->auth))
      {
        msg_error("redisq: REDIS server: failed to authenticate");
        return FALSE;
      }

  if (!_check_connection_to_redis(self))
    {
      msg_error("redisq: REDIS server: failed to connect");
      return FALSE;
    }

  msg_debug("redisq: Connection to REDIS server succeeded");

  return TRUE;
}

static void
_redis_dp_disconnect(LogQueueRedis *self)
{
  msg_debug("redisq: disconnecting from redis server");

  if (self->c)
    redisFree(self->c);
  self->c = NULL;
}

static gint64
_get_length(LogQueue *s)
{
  LogQueueRedis *self = (LogQueueRedis *) s;
  gchar redis_list_name[1024] = "";
  redisReply *reply = NULL;
  glong list_len = 0;

  if (_check_connection_to_redis(self))
    {
      g_snprintf(redis_list_name, sizeof(redis_list_name), "%s_%s", self->redis_options->keyprefix, self->persist_name);

      reply = _get_redis_reply(self, "LLEN %s", redis_list_name);

      if (reply && (reply->type == REDIS_REPLY_INTEGER))
        {
          list_len = reply->integer;
        }
    }

  msg_debug("redisq: get length", evt_tag_int("size", list_len));

  return (list_len / ITEMS_PER_MESSAGE);
}

static void
_push_tail(LogQueue *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogQueueRedis *self = (LogQueueRedis *) s;

  msg_debug("redisq: Pushing msg to tail");

  if (!self->write_message(self, msg, path_options))
    {
      msg_error("redisq: Pushing msg to redis server failed");
    }

  g_static_mutex_lock(&self->super.lock);
  log_queue_push_notify (&self->super);
  log_msg_ref (msg);
  g_static_mutex_unlock(&self->super.lock);

  log_msg_ack (msg, path_options, AT_PROCESSED);
}

static LogMessage *
_pop_head(LogQueue *s, LogPathOptions *path_options)
{
  LogQueueRedis *self = (LogQueueRedis *) s;
  LogMessage *msg = NULL;

  msg_debug("redisq: Pop msg from head");

  msg = self->read_message(self, path_options);

  if (msg != NULL)
    {
      path_options->ack_needed = FALSE;
      log_msg_ack (msg, path_options, AT_PROCESSED);
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
      msg = self->read_message(self, &path_options);

      if (msg != NULL)
        {
          self->delete_message(self);
          log_msg_unref(msg);
        }
    }
}

static void
_rewind_backlog(LogQueue *s, guint rewind_count)
{
  //LogQueueRedis *self = (LogQueueRedis *) s;

  msg_debug("redisq: rewind backlog msg");
}

void
_backlog_all(LogQueue *s)
{
  //LogQueueRedis *self = (LogQueueRedis *) s;

  msg_debug("redisq: backlog all");
}

static void
_free(LogQueue *s)
{
  LogQueueRedis *self = (LogQueueRedis *) s;

  msg_debug("redisq: free up queue");

  g_free(self->persist_name);
  self->persist_name = NULL;
  self->redis_options = NULL;

  log_queue_free_method(&self->super);
  _redis_dp_disconnect(self);
}

static LogMessage *
_read_message(LogQueueRedis *self, LogPathOptions *path_options)
{
  LogMessage *msg = NULL;
  gchar redis_list_name[1024] = "";
  GString *serialized;
  SerializeArchive *sa;
  redisReply *reply = NULL;

  msg_debug("redisq: read message from redis");

  if (!_check_connection_to_redis(self))
    return NULL;

  g_snprintf(redis_list_name, sizeof(redis_list_name), "%s_%s", self->redis_options->keyprefix, self->persist_name);
  reply = _get_redis_reply(self, "LRANGE %s 0 0", redis_list_name);

  if (reply && (reply->elements > 0) && (reply->type == REDIS_REPLY_ARRAY))
    {
      msg_debug("reading from redis server", evt_tag_str("val", reply->element[0]->str));

      serialized = g_string_new_len(reply->element[0]->str, reply->element[0]->len);
      g_string_set_size(serialized, reply->element[0]->len);
      sa = serialize_string_archive_new(serialized);

      msg = log_msg_new_empty();

      if (!log_msg_deserialize(msg, sa))
        msg_error("Can't read correct message from redis server");

      serialize_archive_free(sa);
      g_string_free(serialized, TRUE);
      freeReplyObject(reply);
    }
  else
    {
      msg_debug("redisq: no elements in redis list");
    }

  return msg;
}

static gboolean
_write_message(LogQueueRedis *self, LogMessage *msg, const LogPathOptions *path_options)
{
  GString *serialized;
  gchar redis_list_name[1024] = "";
  SerializeArchive *sa;
  gboolean consumed = FALSE;

  if (_check_connection_to_redis(self))
    {
      msg_debug("redisq: writing msg to redis db");
      serialized = g_string_sized_new(4096);
      sa = serialize_string_archive_new(serialized);
      log_msg_serialize(msg, sa);

      g_snprintf(redis_list_name, sizeof(redis_list_name), "%s_%s", self->redis_options->keyprefix, self->persist_name);

      msg_debug("redisq: serialized msg", evt_tag_str("list", redis_list_name),
                evt_tag_str("msg", serialized->str), evt_tag_int("len", serialized->len));

      consumed = _send_redis_command(self, "LPUSH %s %b", redis_list_name, serialized->str, serialized->len);

      serialize_archive_free(sa);
      g_string_free(serialized, TRUE);
    }

  return consumed;
}

static gboolean
_delete_message(LogQueueRedis *self)
{
  gchar redis_list_name[1024] = "";
  gboolean removed = FALSE;

  if (_check_connection_to_redis(self))
    {
      msg_debug("redisq: removing msg to redis db");
      g_snprintf(redis_list_name, sizeof(redis_list_name), "%s_%s", self->redis_options->keyprefix, self->persist_name);

      removed = _send_redis_command(self, "LPOP %s", redis_list_name);
    }

  return removed;
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

  self->read_message = _read_message;
  self->write_message = _write_message;
  self->delete_message = _delete_message;
}

LogQueue *
log_queue_redis_init_instance(RedisQueueOptions *options, const gchar *persist_name)
{
  LogQueueRedis *self = g_new0(LogQueueRedis, 1);

  msg_debug("redisq: log queue init");

  log_queue_init_instance(&self->super, persist_name);

  self->c = NULL;
  self->redis_options = options;
  self->persist_name = g_strdup(persist_name);

  _set_virtual_functions(self);
  _redis_dp_connect(self, TRUE);
  return &self->super;
}
