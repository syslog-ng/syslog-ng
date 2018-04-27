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
#include <string.h>

#include "java-class-loader.h"
#include "java_machine.h"
#include "messages.h"
#include "reloc.h"

#define SYSLOG_NG_CLASS_LOADER  "org/syslog_ng/SyslogNgClassLoader"
#define SYSLOG_NG_JAR           "syslog-ng-core.jar"

jstring
__create_class_path(ClassLoader *self, JNIEnv *java_env, const gchar *class_path)
{
  GString *g_class_path = g_string_new(get_installation_path_for(SYSLOG_NG_JAVA_MODULE_PATH));
  jstring str_class_path = NULL;
  g_string_append(g_class_path, "/" SYSLOG_NG_JAR);
  if (class_path && (strlen(class_path) > 0))
    {
      g_string_append_c(g_class_path, ':');
      g_string_append(g_class_path, class_path);
    }
  str_class_path = CALL_JAVA_FUNCTION(java_env, NewStringUTF, g_class_path->str);
  g_string_free(g_class_path, TRUE);
  return str_class_path;
}

ClassLoader *
class_loader_new(JNIEnv *java_env)
{
  ClassLoader *self = g_new0(ClassLoader, 1);

  self->syslogng_class_loader = CALL_JAVA_FUNCTION(java_env, FindClass, SYSLOG_NG_CLASS_LOADER);
  if (!self->syslogng_class_loader)
    {
      msg_error("Can't find class",
                evt_tag_str("class_name", SYSLOG_NG_CLASS_LOADER));
      goto error;
    }
  self->loader_constructor = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->syslogng_class_loader, "<init>", "()V");
  if (!self->loader_constructor)
    {
      msg_error("Can't find constructor for SyslogNgClassLoader");
      goto error;
    }

  self->mi_loadclass = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->syslogng_class_loader, "loadClass",
                                          "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Class;");
  if (!self->mi_loadclass)
    {
      msg_error("Can't find method in class",
                evt_tag_str("class_name", SYSLOG_NG_CLASS_LOADER),
                evt_tag_str("method", "Class loadClass(String className)"));
      goto error;
    }

  self->mi_init_current_thread = CALL_JAVA_FUNCTION(java_env, GetMethodID, self->syslogng_class_loader,
                                                    "initCurrentThread", "()V");
  if (!self->mi_init_current_thread)
    {
      msg_error("Can't find method in class",
                evt_tag_str("class_name", SYSLOG_NG_CLASS_LOADER),
                evt_tag_str("method", "void initCurrentThread()"));
    }


  self->loader_object = CALL_JAVA_FUNCTION(java_env, NewObject, self->syslogng_class_loader, self->loader_constructor);
  if (!self->loader_object)
    {
      msg_error("Can't create SyslogNgClassLoader");
      goto error;
    }
  return self;
error:
  class_loader_free(self, java_env);
  return NULL;
}

void class_loader_free(ClassLoader *self, JNIEnv *java_env)
{
  if (!self)
    {
      return;
    }
  if (self->loader_object)
    {
      CALL_JAVA_FUNCTION(java_env, DeleteLocalRef, self->loader_object);
    }
  if (self->syslogng_class_loader)
    {
      CALL_JAVA_FUNCTION(java_env, DeleteLocalRef, self->syslogng_class_loader);
    }
  g_free(self);
  return;
}

jclass
class_loader_load_class(ClassLoader *self, JNIEnv *java_env, const gchar *class_name, const gchar *class_path)
{
  jclass result;
  jstring str_class_name = CALL_JAVA_FUNCTION(java_env, NewStringUTF, class_name);
  jstring str_class_path = __create_class_path(self, java_env, class_path);


  result = CALL_JAVA_FUNCTION(java_env, CallObjectMethod, self->loader_object, self->mi_loadclass, str_class_name,
                              str_class_path);

  CALL_JAVA_FUNCTION(java_env, DeleteLocalRef, str_class_name);
  CALL_JAVA_FUNCTION(java_env, DeleteLocalRef, str_class_path);
  return result;
}

void
class_loader_init_current_thread(ClassLoader *self, JNIEnv *java_env)
{
  CALL_JAVA_FUNCTION(java_env, CallVoidMethod, self->loader_object, self->mi_init_current_thread);
}
