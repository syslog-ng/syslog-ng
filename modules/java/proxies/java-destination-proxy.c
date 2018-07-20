/*
 * Copyright (c) 2010-2014 Balabit
 * Copyright (c) 2010-2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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
  jmethodID mi_send;
  jmethodID mi_send_msg;
  jmethodID mi_open;
  jmethodID mi_close;
  jmethodID mi_is_opened;
  jmethodID mi_flush;
  jmethodID mi_get_name_by_uniq_options;
} JavaDestinationImpl;

struct _JavaDestinationProxy
{
  JavaVMSingleton *java_machine;
  jclass loaded_class;
  JavaDestinationImpl dest_impl;
  LogTemplate *template;
  gint32 *seq_num;
  GString *formatted_message;
  JavaLogMessageProxy *msg_builder;
  gchar *name_by_uniq_options;
};

static gboolean
__load_destination_object(JavaDestinationProxy *self, const gchar *class_name, const gchar *class_path, gpointer handle)
{
  JNIEnv *java_env = java_machine_get_env(self->java_machine);
  self->loaded_class = java_machine_load_class(self->java_machine, class_name, class_path);
  if (!self->loaded_class)
    {
      msg_error("Can't find class",
                evt_tag_str("class_name", class_name));
      return FALSE;
    }

  self->dest_impl.mi_constructor = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "<init>", "(J)V");
  if (!self->dest_impl.mi_constructor)
    {
      msg_error("Can't find default constructor for class",
                evt_tag_str("class_name", class_name));
      return FALSE;
    }

  self->dest_impl.mi_init = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "initProxy", "()Z");
  if (!self->dest_impl.mi_init)
    {
      msg_error("Can't find method in class",
                evt_tag_str("class_name", class_name),
                evt_tag_str("method", "boolean init(SyslogNg)"));
      return FALSE;
    }

  self->dest_impl.mi_deinit = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "deinitProxy", "()V");
  if (!self->dest_impl.mi_deinit)
    {
      msg_error("Can't find method in class",
                evt_tag_str("class_name", class_name),
                evt_tag_str("method", "void deinit()"));
      return FALSE;
    }

  self->dest_impl.mi_send = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "sendProxy",
                                               "(Ljava/lang/String;)Z");
  self->dest_impl.mi_send_msg = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "sendProxy",
                                                   "(Lorg/syslog_ng/LogMessage;)Z");

  if (!self->dest_impl.mi_send_msg && !self->dest_impl.mi_send)
    {
      msg_error("Can't find any queue method in class",
                evt_tag_str("class_name", class_name),
                evt_tag_str("method", "boolean send(String) or boolean send(LogMessage)"));
    }

  self->dest_impl.mi_flush = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class,
      "flushProxy", "()V");
  if (!self->dest_impl.mi_flush)
    {
      msg_error("Can't find method in class",
                evt_tag_str("class_name", class_name),
                evt_tag_str("method", "void flush()"));
      return FALSE;
    }

  self->dest_impl.mi_open = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "openProxy", "()Z");
  if (!self->dest_impl.mi_open)
    {
      msg_error("Can't find method in class",
                evt_tag_str("class_name", class_name),
                evt_tag_str("method", "boolean open()"));
    }

  self->dest_impl.mi_close = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "closeProxy", "()V");
  if (!self->dest_impl.mi_close)
    {
      msg_error("Can't find method in class",
                evt_tag_str("class_name", class_name),
                evt_tag_str("method", "void close()"));
    }

  self->dest_impl.mi_is_opened = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "isOpenedProxy", "()Z");
  if (!self->dest_impl.mi_is_opened)
    {
      msg_error("Can't find method in class",
                evt_tag_str("class_name", class_name),
                evt_tag_str("method", "boolean isOpened()"));
    }

  self->dest_impl.dest_object = CALL_JAVA_FUNCTION(java_env, NewObject, self->loaded_class,
                                                   self->dest_impl.mi_constructor, handle);
  if (!self->dest_impl.dest_object)
    {
      msg_error("Can't create object",
                evt_tag_str("class_name", class_name));
      return FALSE;
    }

  self->dest_impl.mi_get_name_by_uniq_options = CALL_JAVA_FUNCTION(java_env,
      GetMethodID,
      self->loaded_class,
      "getNameByUniqOptionsProxy",
      "()Ljava/lang/String;"
                                                                  );
  if (!self->dest_impl.mi_get_name_by_uniq_options)
    {
      msg_error("Can't get name by unique options",
                evt_tag_str("name", class_name));
      return FALSE;
    }
  return TRUE;
}


void
java_destination_proxy_free(JavaDestinationProxy *self)
{
  JNIEnv *env = java_machine_get_env(self->java_machine);
  if (self->dest_impl.dest_object)
    {
      CALL_JAVA_FUNCTION(env, DeleteLocalRef, self->dest_impl.dest_object);
    }

  if (self->loaded_class)
    {
      CALL_JAVA_FUNCTION(env, DeleteLocalRef, self->loaded_class);
    }

  if (self->msg_builder)
    {
      java_log_message_proxy_free(self->msg_builder);
    }
  java_machine_unref(self->java_machine);
  g_string_free(self->formatted_message, TRUE);
  g_free(self->name_by_uniq_options);
  log_template_unref(self->template);
  g_free(self);
}

JavaDestinationProxy *
java_destination_proxy_new(const gchar *class_name, const gchar *class_path, gpointer handle, LogTemplate *template,
                           gint32 *seq_num, const gchar *jvm_options)
{
  JavaDestinationProxy *self = g_new0(JavaDestinationProxy, 1);
  self->java_machine = java_machine_ref();
  self->formatted_message = g_string_sized_new(1024);
  self->template = log_template_ref(template);
  self->seq_num = seq_num;

  if (!java_machine_start(self->java_machine, jvm_options))
    goto error;

  if (!__load_destination_object(self, class_name, class_path, handle))
    {
      goto error;
    }

  self->msg_builder = java_log_message_proxy_new();
  if (!self->msg_builder)
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
  jobject jmsg = java_log_message_proxy_create_java_object(self->msg_builder, msg);
  jboolean res = CALL_JAVA_FUNCTION(env,
                                    CallBooleanMethod,
                                    self->dest_impl.dest_object,
                                    self->dest_impl.mi_send_msg,
                                    jmsg);

  CALL_JAVA_FUNCTION(env, DeleteLocalRef, jmsg);
  return !!(res);
}

static gboolean
__queue_formatted_message(JavaDestinationProxy *self, JNIEnv *env, LogMessage *msg)
{
  log_template_format(self->template, msg, NULL, LTZ_SEND, *self->seq_num, NULL, self->formatted_message);
  jstring message = CALL_JAVA_FUNCTION(env, NewStringUTF, self->formatted_message->str);
  jboolean res = CALL_JAVA_FUNCTION(env, CallBooleanMethod, self->dest_impl.dest_object, self->dest_impl.mi_send,
                                    message);
  CALL_JAVA_FUNCTION(env, DeleteLocalRef, message);
  return !!(res);
}

gboolean
java_destination_proxy_send(JavaDestinationProxy *self, LogMessage *msg)
{
  JNIEnv *env = java_machine_get_env(self->java_machine);
  if (self->dest_impl.mi_send_msg != 0)
    {
      return __queue_native_message(self, env, msg);
    }
  else
    {
      return __queue_formatted_message(self, env, msg);
    }
}

gchar *
java_destination_proxy_get_name_by_uniq_options(JavaDestinationProxy *self)
{
  return self->name_by_uniq_options;
}

static gchar *
__java_str_dup(JNIEnv *env, jstring java_string)
{
  gchar *result = NULL;
  gchar *c_string = NULL;

  c_string = (gchar *) CALL_JAVA_FUNCTION(env, GetStringUTFChars, java_string, NULL);
  if (strlen(c_string) == 0)
    goto exit;

  result = strdup(c_string);
exit:
  CALL_JAVA_FUNCTION(env, ReleaseStringUTFChars, java_string, c_string);
  return result;
}

static gchar *
__get_name_by_uniq_options(JavaDestinationProxy *self)
{
  JNIEnv *env = java_machine_get_env(self->java_machine);
  jstring java_string;

  java_string = (jstring) CALL_JAVA_FUNCTION(env, CallObjectMethod, self->dest_impl.dest_object,
                                             self->dest_impl.mi_get_name_by_uniq_options);
  if (!java_string)
    {
      msg_error("Can't get name by unique options");
      return NULL;
    }

  return __java_str_dup(env, java_string);
}

gboolean
java_destination_proxy_init(JavaDestinationProxy *self)
{
  jboolean result;
  JNIEnv *env = java_machine_get_env(self->java_machine);

  result = CALL_JAVA_FUNCTION(env, CallBooleanMethod, self->dest_impl.dest_object, self->dest_impl.mi_init);

  if (!!(result))
    {
      self->name_by_uniq_options = __get_name_by_uniq_options(self);
      if (!self->name_by_uniq_options)
        {
          msg_error("Name by uniq options is empty");
          result = FALSE;
        }
    }

  return !!(result);
}

void
java_destination_proxy_deinit(JavaDestinationProxy *self)
{
  JNIEnv *env = java_machine_get_env(self->java_machine);

  CALL_JAVA_FUNCTION(env, CallVoidMethod, self->dest_impl.dest_object, self->dest_impl.mi_deinit);
}

gboolean
java_destination_proxy_flush(JavaDestinationProxy *self)
{
  JNIEnv *env = java_machine_get_env(self->java_machine);

  jboolean res = CALL_JAVA_FUNCTION(env, CallBooleanMethod, self->dest_impl.dest_object, self->dest_impl.mi_flush);

  return !!(res);
}

gboolean
java_destination_proxy_open(JavaDestinationProxy *self)
{
  JNIEnv *env = java_machine_get_env(self->java_machine);

  jboolean res = CALL_JAVA_FUNCTION(env, CallBooleanMethod, self->dest_impl.dest_object, self->dest_impl.mi_open);

  return !!(res);
}

gboolean
java_destination_proxy_is_opened(JavaDestinationProxy *self)
{
  JNIEnv *env = java_machine_get_env(self->java_machine);

  jboolean res = CALL_JAVA_FUNCTION(env, CallBooleanMethod, self->dest_impl.dest_object, self->dest_impl.mi_is_opened);

  return !!(res);
}

void
java_destination_proxy_close(JavaDestinationProxy *self)
{
  JNIEnv *env = java_machine_get_env(self->java_machine);

  CALL_JAVA_FUNCTION(env, CallBooleanMethod, self->dest_impl.dest_object, self->dest_impl.mi_close);

}
