/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2013 Tihamer Petrovics <tihameri@gmail.com>
 * Copyright (c) 2014 Gergely Nagy <algernon@balabit.hu>
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

#include <hiredis/hiredis.h>

#include "redis.h"
#include "redis-parser.h"
#include "plugin.h"
#include "messages.h"
#include "stats/stats-registry.h"
#include "logqueue.h"
#include "driver.h"
#include "plugin-types.h"
#include "logthrdest/logthrdestdrv.h"

typedef struct
{
  LogThreadedDestDriver super;

  gchar *host;
  gint   port;
  gchar *auth;

  LogTemplateOptions template_options;

  GString *command;
  LogTemplate *key;
  GString *key_str;
  LogTemplate *param1;
  GString *param1_str;
  LogTemplate *param2;
  GString *param2_str;

  redisContext *c;
} RedisDriver;

/*
 * Configuration
 */

void
redis_dd_set_host(LogDriver *d, const gchar *host)
{
  RedisDriver *self = (RedisDriver *)d;

  g_free(self->host);
  self->host = g_strdup(host);
}

void
redis_dd_set_port(LogDriver *d, gint port)
{
  RedisDriver *self = (RedisDriver *)d;

  self->port = (int)port;
}

void
redis_dd_set_auth(LogDriver *d, const gchar *auth)
{
  RedisDriver *self = (RedisDriver *)d;

  g_free(self->auth);
  self->auth = g_strdup(auth);
}

void
redis_dd_set_command_ref(LogDriver *d, const gchar *command,
                         LogTemplate *key,
                         LogTemplate *param1, LogTemplate *param2)
{
  RedisDriver *self = (RedisDriver *)d;

  g_string_assign(self->command, command);

  log_template_unref(self->key);
  self->key = key;

  log_template_unref(self->param1);
  self->param1 = param1;

  log_template_unref(self->param2);
  self->param2 = param2;
}

LogTemplateOptions *
redis_dd_get_template_options(LogDriver *d)
{
  RedisDriver *self = (RedisDriver *)d;

  return &self->template_options;
}

/*
 * Utilities
 */

static const gchar *
redis_dd_format_stats_instance(LogThreadedDestDriver *d)
{
  RedisDriver *self = (RedisDriver *)d;
  static gchar persist_name[1024];

  if (d->super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "redis,%s", d->super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "redis,%s,%u", self->host, self->port);

  return persist_name;
}

static const gchar *
redis_dd_format_persist_name(const LogPipe *s)
{
  const RedisDriver *self = (const RedisDriver *)s;
  static gchar persist_name[1024];

  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "redis.%s", s->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "redis(%s,%u)", self->host, self->port);

  return persist_name;
}

static gboolean
send_redis_command(RedisDriver *self, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  redisReply *reply = redisvCommand(self->c, format, ap);
  va_end(ap);

  gboolean retval = reply && (reply->type != REDIS_REPLY_ERROR);
  if (reply)
    freeReplyObject(reply);
  return retval;
}

static gboolean
check_connection_to_redis(RedisDriver *self)
{
  return send_redis_command(self, "ping");
}

static gboolean
authenticate_to_redis(RedisDriver *self, const gchar *password)
{
  return send_redis_command(self, "AUTH %s", self->auth);
}

static gboolean
redis_dd_connect(RedisDriver *self)
{
  if (self->c && check_connection_to_redis(self))
    {
      return TRUE;
    }

  if (self->c)
    redisFree(self->c);

  self->c = redisConnect(self->host, self->port);

  if (self->c->err)
    {
      msg_error("REDIS server error, suspending",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("error", self->c->errstr),
                evt_tag_int("time_reopen", self->super.time_reopen));
      return FALSE;
    }

  if (self->auth)
    if (!authenticate_to_redis(self, self->auth))
      {
        msg_error("REDIS: failed to authenticate");
        return FALSE;
      }

  if (!check_connection_to_redis(self))
    {
      msg_error("REDIS: failed to connect");
      return FALSE;
    }

  msg_debug("Connecting to REDIS succeeded",
            evt_tag_str("driver", self->super.super.super.id));

  return TRUE;
}

static void
redis_dd_disconnect(LogThreadedDestDriver *s)
{
  RedisDriver *self = (RedisDriver *)s;

  if (self->c)
    redisFree(self->c);
  self->c = NULL;
}

/*
 * Worker thread
 */

static LogThreadedResult
redis_worker_insert(LogThreadedDestDriver *s, LogMessage *msg)
{
  RedisDriver *self = (RedisDriver *)s;
  redisReply *reply;
  const char *argv[5];
  size_t argvlen[5];
  int argc = 2;

  if (!redis_dd_connect(self))
    return LTR_NOT_CONNECTED;

  if (self->c->err)
    return LTR_ERROR;

  if (!check_connection_to_redis(self))
    {
      msg_error("REDIS: worker failed to connect");
      return LTR_NOT_CONNECTED;
    }

  log_template_format(self->key, msg, &self->template_options, LTZ_SEND,
                      self->super.worker.instance.seq_num, NULL, self->key_str);

  if (self->param1)
    log_template_format(self->param1, msg, &self->template_options, LTZ_SEND,
                        self->super.worker.instance.seq_num, NULL, self->param1_str);
  if (self->param2)
    log_template_format(self->param2, msg, &self->template_options, LTZ_SEND,
                        self->super.worker.instance.seq_num, NULL, self->param2_str);

  argv[0] = self->command->str;
  argvlen[0] = self->command->len;
  argv[1] = self->key_str->str;
  argvlen[1] = self->key_str->len;

  if (self->param1)
    {
      argv[2] = self->param1_str->str;
      argvlen[2] = self->param1_str->len;
      argc++;
    }

  if (self->param2)
    {
      argv[3] = self->param2_str->str;
      argvlen[3] = self->param2_str->len;
      argc++;
    }

  reply = redisCommandArgv(self->c, argc, argv, argvlen);

  if (!reply)
    {
      msg_error("REDIS server error, suspending",
                evt_tag_str("driver", self->super.super.super.id),
                evt_tag_str("command", self->command->str),
                evt_tag_str("key", self->key_str->str),
                evt_tag_str("param1", self->param1_str->str),
                evt_tag_str("param2", self->param2_str->str),
                evt_tag_str("error", self->c->errstr),
                evt_tag_int("time_reopen", self->super.time_reopen));
      return LTR_ERROR;
    }

  msg_debug("REDIS command sent",
            evt_tag_str("driver", self->super.super.super.id),
            evt_tag_str("command", self->command->str),
            evt_tag_str("key", self->key_str->str),
            evt_tag_str("param1", self->param1_str->str),
            evt_tag_str("param2", self->param2_str->str));
  freeReplyObject(reply);

  return LTR_SUCCESS;
}

static void
redis_worker_thread_init(LogThreadedDestDriver *d)
{
  RedisDriver *self = (RedisDriver *)d;

  msg_debug("Worker thread started",
            evt_tag_str("driver", self->super.super.super.id));

  self->key_str = g_string_sized_new(1024);
  self->param1_str = g_string_sized_new(1024);
  self->param2_str = g_string_sized_new(1024);

  redis_dd_connect(self);
}

static void
redis_worker_thread_deinit(LogThreadedDestDriver *d)
{
  RedisDriver *self = (RedisDriver *)d;

  g_string_free(self->key_str, TRUE);
  g_string_free(self->param1_str, TRUE);
  g_string_free(self->param2_str, TRUE);

  redis_dd_disconnect(d);
}

/*
 * Main thread
 */

static gboolean
redis_dd_init(LogPipe *s)
{
  RedisDriver *self = (RedisDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_threaded_dest_driver_init_method(s))
    return FALSE;

  if (!self->key && !self->param1)
    {
      msg_error("Error initializing Redis destination, command option MUST be set",
                log_pipe_location_tag(s));
      return FALSE;
    }

  log_template_options_init(&self->template_options, cfg);

  msg_verbose("Initializing Redis destination",
              evt_tag_str("driver", self->super.super.super.id),
              evt_tag_str("host", self->host),
              evt_tag_int("port", self->port));

  return log_threaded_dest_driver_start_workers(&self->super);
}

static void
redis_dd_free(LogPipe *d)
{
  RedisDriver *self = (RedisDriver *)d;

  log_template_options_destroy(&self->template_options);

  g_free(self->host);
  g_free(self->auth);
  g_string_free(self->command, TRUE);
  log_template_unref(self->key);
  log_template_unref(self->param1);
  log_template_unref(self->param2);
  if (self->c)
    redisFree(self->c);

  log_threaded_dest_driver_free(d);
}

/*
 * Plugin glue.
 */

LogDriver *
redis_dd_new(GlobalConfig *cfg)
{
  RedisDriver *self = g_new0(RedisDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);
  self->super.super.super.super.init = redis_dd_init;
  self->super.super.super.super.free_fn = redis_dd_free;
  self->super.super.super.super.generate_persist_name = redis_dd_format_persist_name;

  self->super.worker.thread_init = redis_worker_thread_init;
  self->super.worker.thread_deinit = redis_worker_thread_deinit;
  self->super.worker.disconnect = redis_dd_disconnect;
  self->super.worker.insert = redis_worker_insert;

  self->super.format_stats_instance = redis_dd_format_stats_instance;
  self->super.stats_source = stats_register_type("redis");

  redis_dd_set_host((LogDriver *)self, "127.0.0.1");
  redis_dd_set_port((LogDriver *)self, 6379);
  redis_dd_set_auth((LogDriver *)self, NULL);

  self->command = g_string_sized_new(32);

  log_template_options_defaults(&self->template_options);

  return (LogDriver *)self;
}

extern CfgParser redis_dd_parser;

static Plugin redis_plugin =
{
  .type = LL_CONTEXT_DESTINATION,
  .name = "redis",
  .parser = &redis_parser,
};

gboolean
redis_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, &redis_plugin, 1);

  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "redis",
  .version = SYSLOG_NG_VERSION,
  .description = "The afredis module provides Redis destination support for syslog-ng.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = &redis_plugin,
  .plugins_len = 1,
};
