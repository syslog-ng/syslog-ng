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
#include "stats.h"
#include "logqueue.h"
#include "driver.h"

#include <stdio.h>

void java_dd_start_watches(JavaDestDriver *self);

void java_dd_set_option(LogDriver *s, const gchar *key, const gchar *value)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  g_hash_table_insert(self->options, g_strdup(key), g_strdup(value));
}

JNIEXPORT jstring JNICALL Java_org_syslog_1ng_LogPipe_getOption(JNIEnv *env, jobject obj, jlong s, jstring key)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  gchar *value;
  const char *key_str = (*env)->GetStringUTFChars(env, key, NULL);
  if (key_str == NULL)
    {
      return NULL;
    }

  value = g_hash_table_lookup(self->options, key_str);
  (*env)->ReleaseStringUTFChars(env, key, key_str);  // release resources

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
Java_org_syslog_1ng_SyslogNgDestination_getConfigHandle(JNIEnv *env, jobject obj, jlong handle)
{
  JavaDestDriver *self = (JavaDestDriver *)handle;
  return (jlong)log_pipe_get_config(&self->super.super.super);
}

static void
java_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  log_queue_push_tail(self->log_queue, msg, path_options);
}

void
java_dd_set_class_path(LogDriver *s, const gchar *class_path)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  g_assert(!(self->super.super.super.flags & PIF_INITIALIZED));
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
  if (!java_machine_start(self->java_machine, &self->java_env))
    return FALSE;

  self->proxy = java_destination_proxy_new(self->class_name, self->class_path->str, self, self->template);
  if (!self->proxy)
    return FALSE;
  self->log_queue = log_dest_driver_acquire_queue(&self->super, "testjava");
  log_queue_set_use_backlog(self->log_queue, TRUE);
  java_dd_start_watches(self);
  return java_destination_proxy_init(self->proxy, self->java_env, self);
}

gboolean
java_dd_deinit(LogPipe *s)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  java_destination_proxy_deinit(self->proxy, self->java_env);
  return TRUE;
}

void
java_dd_wake_up(gpointer user_data)
{
  JavaDestDriver *self = (JavaDestDriver *)user_data;
  iv_event_post(&self->wake_up_event);
}

void
java_dd_stop_watches(JavaDestDriver *self)
{
  log_queue_reset_parallel_push(self->log_queue);
}

gboolean
java_dd_send_to_object(JavaDestDriver *self, LogMessage *msg, JNIEnv *env)
{
  return java_destination_proxy_queue(self->proxy, env, msg);
}

void
java_dd_work_perform(gpointer data)
{
  JavaDestDriver *self = (JavaDestDriver *)data;
  gboolean sent = TRUE;
  JNIEnv *env = NULL;

  env = java_machine_get_env(self->java_machine, &env);
  while (sent && !main_loop_worker_job_quit())
    {
      LogMessage *lm;
      LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

      lm = log_queue_pop_head(self->log_queue, &path_options);
      if (!lm)
        {
          /* no more items are available */
          break;
        }

      log_msg_refcache_start_consumer(lm, &path_options);
      msg_set_context(lm);
      sent = java_dd_send_to_object(self, lm, env);
      if (sent)
        log_queue_ack_backlog(self->log_queue, 1);
      else
        log_queue_rewind_backlog(self->log_queue, 1);
      log_msg_unref(lm);
      msg_set_context(NULL);
      log_msg_refcache_stop();
    }

  java_destination_proxy_flush(self->proxy, env);

  if (!main_loop_is_main_thread())
    {
      java_machine_detach_thread(self->java_machine);
    }


}

void
java_dd_work_finished(gpointer data)
{
  JavaDestDriver *self = (JavaDestDriver *)data;
  if (self->super.super.super.flags & PIF_INITIALIZED)
    {
      java_dd_start_watches(self);
    }
  log_pipe_unref(&self->super.super.super);
}

void
java_dd_process_output(JavaDestDriver *self)
{
  main_loop_assert_main_thread();

  java_dd_stop_watches(self);
  log_pipe_ref(&self->super.super.super);
  if (self->threaded)
    {
      main_loop_io_worker_job_submit(&self->io_job);
    }
  else
    {
      if (!main_loop_worker_job_quit())
        {
          java_dd_work_perform(self);
          java_dd_work_finished(self);
        }
    }

}

void
java_dd_update_watches(gpointer cookie)
{
  JavaDestDriver *self = (JavaDestDriver *)cookie;
  if (log_queue_check_items(self->log_queue, NULL, java_dd_wake_up, self, NULL))
    {
      java_dd_process_output(self);
    }
}

void
java_dd_start_watches(JavaDestDriver *self)
{
  java_dd_update_watches(self);
}

void
java_dd_init_watches(JavaDestDriver *self)
{
  IV_EVENT_INIT(&self->wake_up_event);
  self->wake_up_event.cookie = self;
  self->wake_up_event.handler = java_dd_update_watches;
  iv_event_register(&self->wake_up_event);

  main_loop_io_worker_job_init(&self->io_job);
  self->io_job.user_data = self;
  self->io_job.work = (void (*)(void *)) java_dd_work_perform;
  self->io_job.completion = (void (*)(void *)) java_dd_work_finished;
}

void
java_dd_free(LogPipe *s)
{
  JavaDestDriver *self = (JavaDestDriver *)s;
  log_template_unref(self->template);
  if (self->proxy)
    java_destination_proxy_free(self->proxy);

  if (self->java_machine)
    {
      java_machine_unref(self->java_machine);
      self->java_machine = NULL;
    }
  g_free(self->class_name);
  g_hash_table_unref(self->options);
  g_string_free(self->class_path, TRUE);
}

LogDriver *
java_dd_new(GlobalConfig *cfg)
{
  JavaDestDriver *self = g_new0(JavaDestDriver, 1);

  log_dest_driver_init_instance(&self->super);
  self->super.super.super.free_fn = java_dd_free;
  self->super.super.super.init = java_dd_init;
  self->super.super.super.deinit = java_dd_deinit;
  self->super.super.super.queue = java_dd_queue;

  self->template = log_template_new(cfg, "java_dd_template");
  self->class_path = g_string_new(".");
  self->java_machine = java_machine_ref();

  java_dd_set_template_string(&self->super.super, "$ISODATE $HOST $MSGHDR$MSG");
  self->threaded = cfg->threaded;
  self->formatted_message = g_string_sized_new(1024);
  self->options = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  java_dd_init_watches(self);
  return (LogDriver *)self;
}
