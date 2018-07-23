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
#include "logmsg/logmsg-serialize.h"
#include <criterion/criterion.h>
#include "plugin.h"
#include "apphook.h"
#include "queue_utils_lib.h"
#include "redisq-options.h"

#define ITEMS_PER_MESSAGE 2

typedef struct _FakeRedisQueue FakeRedisQueue;

struct _FakeRedisQueue
{
  LogQueueRedis super;
  RedisQueueOptions options;
};

static FakeRedisQueue fakeRedisQ;
static LogMessage *test_msg = NULL;

static LogMessage *
_construct_msg(const gchar *msg)
{
  LogMessage *logmsg;

  logmsg = log_msg_new_empty();
  log_msg_set_value_by_name(logmsg, "MESSAGE", msg, -1);
  return logmsg;
}

void *redisvCommand(redisContext *c, const char *format, va_list ap)
{
  redisReply *reply = g_new0(redisReply, 1);
  GString *serialized;
  SerializeArchive *sa;

  if (strstr(format, "RPUSH"))
    {
      test_msg = _construct_msg("Hello redis queue");
    }
  else if (strstr(format, "LRANGE") && test_msg)
    {
      serialized = g_string_sized_new(4096);
      sa = serialize_string_archive_new(serialized);
      log_msg_serialize(test_msg, sa);

      reply->elements = 1;
      reply->type = REDIS_REPLY_ARRAY;
      reply->element = g_malloc0(sizeof(redisReply *));
      reply->element[0] = g_new0(redisReply, 1);
      reply->element[0]->str = g_malloc0(2048);

      memcpy(reply->element[0]->str, serialized->str, serialized->len);
      reply->element[0]->len = serialized->len;

      serialize_archive_free(sa);
      g_string_free(serialized, TRUE);

      return reply;
    }

  return NULL;
}

void freeReplyObject(void *reply)
{
  redisReply *rep = (redisReply *) reply;

  printf("freeReplyObject\n");

  if (rep->elements > 0)
    {
      g_free(rep->element[0]->str);
      g_free(rep->element[0]);
      g_free(rep->element);
    }

  g_free(reply);
}

static gboolean
_is_conn_alive(LogQueueRedis *self)
{
  return TRUE;
}

static LogQueue *
_fake_logq_redis_new(FakeRedisQueue *self)
{
  const gchar *persist_name = "test_redisq";
  LogQueue *q;

  redis_queue_options_set_default_options(&self->options);
  self->super.redis_options = &self->options;
  self->super.check_conn = _is_conn_alive;
  self->super.redis_thread_mutex = g_mutex_new();

  q = log_queue_redis_new(&self->super, persist_name);
  log_queue_set_use_backlog(q, TRUE);

  return q;
}

static void
_fake_logq_redis_free(FakeRedisQueue *self)
{
  redis_queue_options_destroy(&self->options);
  g_static_mutex_free(&self->super.super.lock);
  g_free(self->super.super.persist_name);
}

Test(redisq, test_push_pop_msg)
{
  GString *serialized, *out_serialized;
  SerializeArchive *sa, *out_sa;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogMessage *msg, *out_msg = NULL;
  LogQueue *q;

  q = _fake_logq_redis_new(&fakeRedisQ);

  msg = _construct_msg("Hello redis queue");
  log_queue_push_tail(q, msg, &path_options);
  out_msg = log_queue_pop_head(q, &path_options);

  serialized = g_string_sized_new(4096);
  sa = serialize_string_archive_new(serialized);
  log_msg_serialize(test_msg, sa);

  out_serialized = g_string_sized_new(4096);
  out_sa = serialize_string_archive_new(out_serialized);
  log_msg_serialize(out_msg, out_sa);

  cr_assert_eq(memcmp(serialized->str, out_serialized->str, serialized->len), 0, "Can't read msg from queue");

  serialize_archive_free(sa);
  g_string_free(serialized, TRUE);

  serialize_archive_free(out_sa);
  g_string_free(out_serialized, TRUE);

  log_msg_unref(msg);
  log_msg_unref(out_msg);
  _fake_logq_redis_free(&fakeRedisQ);
}

Test(redisq, test_pop_empty_msg)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogQueue *q;
  LogMessage *msg;

  q = _fake_logq_redis_new(&fakeRedisQ);
  msg = log_queue_pop_head(q, &path_options);

  cr_assert_null(msg, "Queue is not empty");

  _fake_logq_redis_free(&fakeRedisQ);
}

#if 0

Test(redisq, test_rewind_backlog)
{
  guint qlen = 0;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  LogQueue *q;
  LogMessage *mark_msg = log_msg_new_mark();
  LogMessage *backlog_msg;

  q = _fake_logq_redis_new(&fakeRedisQ);

  log_queue_push_tail(q, mark_msg, &path_options);
  log_queue_pop_head(q, &path_options);
  log_queue_rewind_backlog(q, 1);

// qlen = _get_length_from_qredis(NULL);
// cr_assert_eq(qlen, 1, "Queue is empty");

  backlog_msg = log_queue_pop_head(q, &path_options);
  cr_assert_eq(mark_msg, backlog_msg, "Can't read message from backlog");

  log_msg_unref(mark_msg);
  _fake_logq_redis_free(&fakeRedisQ);
}

Test(redisq, test_multiple_msgs)
{

}
#endif

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  app_shutdown();
}

TestSuite(redisq, .init = setup, .fini = teardown);
