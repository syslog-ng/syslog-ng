/*
 * Copyright (c) 2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "java-destination.h"
#include "messages.h"
#include "stats/stats.h"
#include "logqueue.h"
#include "driver.h"

#include <stdio.h>

#ifndef SCS_JAVA
#define SCS_JAVA 0
#endif

gchar *
replace_char(gchar *buffer,gchar from,gchar to)
{
  gchar *p;
  p = buffer;
  while (*p)
    {
      if (*p == from)
        *p = to;
      p++;
    }
  return buffer;
}

JNIEXPORT jstring JNICALL
Java_org_syslog_1ng_LogDestination_getOption(JNIEnv *env, jobject obj, jlong s, jstring key)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  gchar *value;
  const char *key_str = (*env)->GetStringUTFChars(env, key, NULL);
  if (key_str == NULL)
    {
      return NULL;
    }
  gchar *normalized_key = g_strdup(key_str);
  normalized_key = replace_char(normalized_key, '-', '_');
  value = g_hash_table_lookup(self->options, normalized_key);
  (*env)->ReleaseStringUTFChars(env, key, key_str);  // release resources
  g_free(normalized_key);

  if (value)
    {
      return (*env)->NewStringUTF(env, value);
    }
  else
    {
      return NULL;
    }
}

JNIEXPORT jlong JNICALL
Java_org_syslog_1ng_LogPipe_getConfigHandle(JNIEnv *env, jobject obj, jlong handle)
{
  JavaDestDriver *self = (JavaDestDriver *)handle;
  return (jlong)log_pipe_get_config(&self->super.super.super.super);
}

void java_dd_set_option(LogDriver *s, const gchar *key, const gchar *value)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  gchar *normalized_key = g_strdup(key);
  normalized_key = replace_char(normalized_key, '-', '_');
  g_hash_table_insert(self->options, g_strdup(normalized_key), g_strdup(value));
}


void
java_dd_set_class_path(LogDriver *s, const gchar *class_path)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  g_string_assign(self->class_path, class_path);
}

void
java_dd_set_class_name(LogDriver *s, const gchar *class_name)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  g_free(self->class_name);
  self->class_name = g_strdup(class_name);
}

void
java_dd_set_template_string(LogDriver *s, const gchar *template_string)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  g_free(self->template_string);
  self->template_string = g_strdup(template_string);
}

gboolean
java_dd_init(LogPipe *s)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  GError *error = NULL;

  if (!log_dest_driver_init_method(s))
    return FALSE;

  if (!log_template_compile(self->template, self->template_string, &error))
    {
      msg_error("Can't compile template",
                evt_tag_str("template", self->template_string),
                evt_tag_str("error", error->message),
                NULL
      );
      return FALSE;
    }

  self->proxy = java_destination_proxy_new(self->class_name, self->class_path->str, self, self->template);
  if (!self->proxy)
    return FALSE;

  if (!java_destination_proxy_init(self->proxy))
    return FALSE;

  return log_threaded_dest_driver_start(s);
}

gboolean
java_dd_deinit(LogPipe *s)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  java_destination_proxy_deinit(self->proxy);
  return log_threaded_dest_driver_deinit_method(s);
}

gboolean
java_dd_send_to_object(JavaDestDriver *self, LogMessage *msg)
{
  return java_destination_proxy_send(self->proxy, msg);
}

gboolean
java_dd_open(JavaDestDriver *self)
{
  if (!java_destination_proxy_is_opened(self->proxy))
    {
      return java_destination_proxy_open(self->proxy);
    }
  return TRUE;
}

void
java_dd_close(JavaDestDriver *self)
{
  if (java_destination_proxy_is_opened(self->proxy))
    {
      java_destination_proxy_close(self->proxy);
    }
}

static worker_insert_result_t
java_worker_insert(LogThrDestDriver *s, LogMessage *msg)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  if (!java_dd_open(self))
    {
      return WORKER_INSERT_RESULT_NOT_CONNECTED;
    }
  gboolean sent = java_dd_send_to_object(self, msg);
  return sent ? WORKER_INSERT_RESULT_SUCCESS : WORKER_INSERT_RESULT_ERROR;
}

static void
java_worker_thread_deinit(LogThrDestDriver *d)
{
  JavaDestDriver *self = (JavaDestDriver *)d;
  java_dd_close(self);
  java_machine_detach_thread();
}

static gchar *
java_dd_format_stats_instance(LogThrDestDriver *d)
{
  JavaDestDriver *self = (JavaDestDriver *)d;
  static gchar persist_name[1024];


  g_snprintf(persist_name, sizeof(persist_name),
               "java,%s", self->class_name);
  return persist_name;
}

void
java_dd_free(LogPipe *s)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  log_template_unref(self->template);
  if (self->proxy)
    java_destination_proxy_free(self->proxy);

  g_free(self->class_name);
  g_hash_table_unref(self->options);
  g_string_free(self->class_path, TRUE);
}

static void
__retry_over_message(LogThrDestDriver *s, LogMessage *msg)
{
  msg_error("Multiple failures while inserting this record to the java destination, message dropped",
            evt_tag_int("number_of_retries", s->retries.max),
            NULL);
}

LogDriver *
java_dd_new(GlobalConfig *cfg)
{
  JavaDestDriver *self = g_new0(JavaDestDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);
  self->super.super.super.super.free_fn = java_dd_free;
  self->super.super.super.super.init = java_dd_init;
  self->super.super.super.super.deinit = java_dd_deinit;

  self->super.worker.thread_deinit = java_worker_thread_deinit;
  self->super.worker.insert = java_worker_insert;

  self->super.format.stats_instance = java_dd_format_stats_instance;
  self->super.format.persist_name = java_dd_format_stats_instance;
  self->super.messages.retry_over = __retry_over_message;
  self->super.stats_source = SCS_JAVA;

  self->template = log_template_new(cfg, "java_dd_template");
  self->class_path = g_string_new(".");

  java_dd_set_template_string(&self->super.super.super, "$ISODATE $HOST $MSGHDR$MSG\n");

  self->formatted_message = g_string_sized_new(1024);
  self->options = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  return (LogDriver *)self;
}
