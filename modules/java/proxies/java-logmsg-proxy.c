/*
 * Copyright (c) 2010-2015 Balabit
 * Copyright (c) 2010-2015 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "java-logmsg-proxy.h"
#include "java_machine.h"
#include "messages.h"
#include "str-utils.h"
#include "logmsg/logmsg.h"


#define LOG_MESSAGE "org.syslog_ng.LogMessage"

struct _JavaLogMessageProxy
{
  JavaVMSingleton *java_machine;
  jclass loaded_class;
  jmethodID mi_constructor;
};

JNIEXPORT void JNICALL
Java_org_syslog_1ng_LogMessage_unref(JNIEnv *env, jobject obj, jlong handle)
{
  LogMessage *msg = (LogMessage *)handle;
  log_msg_unref(msg);
}

JNIEXPORT jstring JNICALL
Java_org_syslog_1ng_LogMessage_getValue(JNIEnv *env, jobject obj, jlong handle, jstring name)
{
  LogMessage *msg = (LogMessage *)handle;
  const gchar *value;
  gssize value_len;

  const char *name_str = (*env)->GetStringUTFChars(env, name, NULL);
  if (name_str == NULL)
    {
      return NULL;
    }

  value = log_msg_get_value_by_name(msg, name_str, &value_len);

  (*env)->ReleaseStringUTFChars(env, name, name_str);

  if (value)
    {
      APPEND_ZERO(value, value, value_len);
      return (*env)->NewStringUTF(env, value);
    }
  else
    {
      return NULL;
    }
}

static gboolean
__load_object(JavaLogMessageProxy *self)
{
  JNIEnv *java_env = java_machine_get_env(self->java_machine);
  self->loaded_class = java_machine_load_class(self->java_machine, LOG_MESSAGE, NULL);
  if (!self->loaded_class)
    {
      msg_error("Can't find class",
                evt_tag_str("class_name", LOG_MESSAGE));
      return FALSE;
    }

  self->mi_constructor = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->loaded_class, "<init>", "(J)V");
  if (!self->mi_constructor)
    {
      msg_error("Can't find default constructor for class",
                evt_tag_str("class_name", LOG_MESSAGE));
      return FALSE;
    }

  return TRUE;
}

jobject
java_log_message_proxy_create_java_object(JavaLogMessageProxy *self, LogMessage *msg)
{
  JNIEnv *java_env = java_machine_get_env(self->java_machine);
  jobject jmsg = CALL_JAVA_FUNCTION(java_env, NewObject, self->loaded_class, self->mi_constructor, log_msg_ref(msg));
  if (!jmsg)
    {
      msg_error("Can't create object",
                evt_tag_str("class_name", LOG_MESSAGE));
    }
  return jmsg;
}

void
java_log_message_proxy_free(JavaLogMessageProxy *self)
{
  JNIEnv *env = java_machine_get_env(self->java_machine);

  if (self->loaded_class)
    {
      CALL_JAVA_FUNCTION(env, DeleteLocalRef, self->loaded_class);
    }
  java_machine_unref(self->java_machine);
  g_free(self);
}

JavaLogMessageProxy *
java_log_message_proxy_new(void)
{
  JavaLogMessageProxy *self = g_new0(JavaLogMessageProxy, 1);
  self->java_machine = java_machine_ref();

  if (!__load_object(self))
    {
      goto error;
    }

  return self;
error:
  java_log_message_proxy_free(self);
  return NULL;
}



