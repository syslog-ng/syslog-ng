/*
 * Copyright (c) 2010-2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2010-2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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


#include "java-destination-proxy.h"
#include "java-logmsg-proxy.h"
#include "java-class-loader.h"
#include "messages.h"
#include <string.h>


typedef struct _JavaDestinationImpl
{
  jobject dest_object;
  jmethodID mi_constructor;
  jmethodID mi_init;
  jmethodID mi_deinit;
  jmethodID mi_queue;
  jmethodID mi_queue_msg;
  jmethodID mi_flush;
} JavaDestinationImpl;

struct _JavaDestinationProxy
{
  JavaVMSingleton *java_machine;
  jclass loaded_class;
  JavaDestinationImpl dest_impl;
  LogTemplate *template;
  GString *formatted_message;
};


static gboolean
__load_destination_object(JavaDestinationProxy *self, const gchar *class_name, const gchar *class_path, gpointer impl)
{
  JNIEnv *java_env = NULL;
  java_env = java_machine_get_env(self->java_machine, &java_env);
  self->loaded_class = java_machine_load_class(self->java_machine, class_name, class_path);
  if (!self->loaded_class) {
      msg_error("Can't find class",
                evt_tag_str("class_name", class_name),
                NULL);
      return FALSE;
  }

  self->dest_impl.mi_constructor = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "<init>", "(J)V");
  if (!self->dest_impl.mi_constructor) {
      msg_error("Can't find default constructor for class",
                evt_tag_str("class_name", class_name), NULL);
      return FALSE;
  }

  self->dest_impl.mi_init = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "init", "()Z");
  if (!self->dest_impl.mi_init) {
      msg_error("Can't find method in class",
                evt_tag_str("class_name", class_name),
                evt_tag_str("method", "boolean init(SyslogNg)"), NULL);
      return FALSE;
  }

  self->dest_impl.mi_deinit = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "deinit", "()V");
  if (!self->dest_impl.mi_deinit) {
      msg_error("Can't find method in class",
                evt_tag_str("class_name", class_name),
                evt_tag_str("method", "void deinit()"), NULL);
      return FALSE;
  }

  self->dest_impl.mi_queue = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "queue", "(Ljava/lang/String;)Z");
  self->dest_impl.mi_queue_msg = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "queue", "(Lorg/syslog_ng/LogMessage;)Z");

  if (!self->dest_impl.mi_queue_msg && !self->dest_impl.mi_queue)
    {
      msg_error("Can't find any queue method in class",
                evt_tag_str("class_name", class_name),
                evt_tag_str("method", "boolean queue(String) or boolean queue(LogMessage)"),
                NULL);
    }

  self->dest_impl.mi_flush = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "flush", "()Z");
  if (!self->dest_impl.mi_flush)
    {
      msg_error("Can't find method in class",
                evt_tag_str("class_name", class_name),
                evt_tag_str("method", "boolean flush()"), NULL);
      return FALSE;
    }

  self->dest_impl.dest_object = CALL_JAVA_FUNCTION(java_env, NewObject, self->loaded_class, self->dest_impl.mi_constructor, impl);
  if (!self->dest_impl.dest_object)
    {
      msg_error("Can't create object",
                evt_tag_str("class_name", class_name),
                NULL);
      return FALSE;
    }
  return TRUE;
}


void
java_destination_proxy_free(JavaDestinationProxy *self)
{
  JNIEnv *env = NULL;
  env = java_machine_get_env(self->java_machine, &env);
  if (self->dest_impl.dest_object)
    {
      CALL_JAVA_FUNCTION(env, DeleteLocalRef, self->dest_impl.dest_object);
    }

  if (self->loaded_class)
    {
      CALL_JAVA_FUNCTION(env, DeleteLocalRef, self->loaded_class);
    }
  java_machine_unref(self->java_machine);
  g_string_free(self->formatted_message, TRUE);
  log_template_unref(self->template);
  g_free(self);
}

JavaDestinationProxy *
java_destination_proxy_new(const gchar *class_name, const gchar *class_path, gpointer impl, LogTemplate *template)
{
  JavaDestinationProxy *self = g_new0(JavaDestinationProxy, 1);
  self->java_machine = java_machine_ref();
  self->formatted_message = g_string_sized_new(1024);
  self->template = log_template_ref(template);

  if (!__load_destination_object(self, class_name, class_path, impl))
    {
      goto error;
    }

  return self;
error:
  java_destination_proxy_free(self);
  return NULL;
}

static gboolean
__queue_native_message(JavaDestinationProxy *self, JNIEnv *env, LogMessage *msg)
{
  JavaLogMessageProxy *jmsg = java_log_message_proxy_new(msg);
  if (!jmsg)
    {
      return FALSE;
    }
  jobject obj = java_log_message_proxy_get_java_object(jmsg);
  jboolean res = CALL_JAVA_FUNCTION(env,
                                    CallBooleanMethod,
                                    self->dest_impl.dest_object,
                                    self->dest_impl.mi_queue_msg,
                                    java_log_message_proxy_get_java_object(jmsg));
  java_log_message_proxy_free(jmsg);
  return !!(res);
}

static gboolean
__queue_formatted_message(JavaDestinationProxy *self, JNIEnv *env, LogMessage *msg)
{
  log_template_format(self->template, msg, NULL, LTZ_LOCAL, 0, NULL, self->formatted_message);
  jstring message = CALL_JAVA_FUNCTION(env, NewStringUTF, self->formatted_message->str);
  jboolean res = CALL_JAVA_FUNCTION(env, CallBooleanMethod, self->dest_impl.dest_object, self->dest_impl.mi_queue, message);
  CALL_JAVA_FUNCTION(env, DeleteLocalRef, message);
  return !!(res);
}

gboolean
java_destination_proxy_queue(JavaDestinationProxy *self, JNIEnv *env, LogMessage *msg)
{
  if (self->dest_impl.mi_queue_msg != 0)
    {
      return __queue_native_message(self, env, msg);
    }
  else
    {
      return __queue_formatted_message(self, env, msg);
    }
}

gboolean
java_destination_proxy_init(JavaDestinationProxy *self, JNIEnv *env, void *ptr)
{
  gboolean result;

  result = CALL_JAVA_FUNCTION(env, CallBooleanMethod, self->dest_impl.dest_object, self->dest_impl.mi_init);
  if (!result)
    {
      goto error;
    }
  return TRUE;
error:
   (*env)->ExceptionDescribe(env);
   return FALSE;
}

void
java_destination_proxy_deinit(JavaDestinationProxy *self, JNIEnv *env)
{
  CALL_JAVA_FUNCTION(env, CallVoidMethod, self->dest_impl.dest_object, self->dest_impl.mi_deinit);
}

gboolean
java_destination_proxy_flush(JavaDestinationProxy *self, JNIEnv *env)
{
  return CALL_JAVA_FUNCTION(env, CallBooleanMethod, self->dest_impl.dest_object, self->dest_impl.mi_flush);
}


