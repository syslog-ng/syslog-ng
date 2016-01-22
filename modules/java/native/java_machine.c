/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "java_machine.h"
#include "java-class-loader.h"
#include "syslog-ng.h"
#include "messages.h"
#include "atomic.h"
#include "lib/reloc.h"
#include "plugin.h"

struct _JavaVMSingleton
{
  GAtomicCounter ref_cnt;
  JavaVMOption options[3];
  JNIEnv *env;
  JavaVM *jvm;
  JavaVMInitArgs vm_args;
  GString *class_path;
  ClassLoader *loader;
};

static JavaVMSingleton *g_jvm_s;

JavaVMSingleton *
java_machine_ref(void)
{
  if (g_jvm_s)
    {
      g_atomic_counter_inc(&g_jvm_s->ref_cnt);
    }
  else
    {
      g_jvm_s = g_new0(JavaVMSingleton, 1);
      g_atomic_counter_set(&g_jvm_s->ref_cnt, 1);

      g_jvm_s->class_path = g_string_new(get_installation_path_for(SYSLOG_NG_JAVA_MODULE_PATH));
      g_string_append(g_jvm_s->class_path, "/syslog-ng-core.jar");
    }
  return g_jvm_s;
}

static void inline
__jvm_free(JavaVMSingleton *self)
{
  msg_debug("Java machine free", NULL);
  g_string_free(self->class_path, TRUE);
  if (self->jvm)
    {
      JavaVM jvm = *(self->jvm);
      if (self->loader)
        {
          JNIEnv *env;
          class_loader_free(self->loader, java_machine_get_env(self, &env));
        }
      jvm->DestroyJavaVM(self->jvm);

    }
  g_free(self);
  g_jvm_s = NULL;
}

void
java_machine_unref(JavaVMSingleton *self)
{
  g_assert(self == g_jvm_s);
  if (g_atomic_counter_dec_and_test(&self->ref_cnt))
    {
      __jvm_free(self);
    }
}

gboolean
java_machine_start(JavaVMSingleton* self)
{
  g_assert(self == g_jvm_s);
  if (!self->jvm)
    {
      long status;
      self->options[0].optionString = g_strdup_printf(
          "-Djava.class.path=%s", self->class_path->str);

      self->options[1].optionString = g_strdup_printf(
          "-Djava.library.path=%s", initial_module_path);

      self->options[2].optionString = g_strdup("-Xrs");

      self->vm_args.version = JNI_VERSION_1_6;
      self->vm_args.nOptions = 3;
      self->vm_args.options = self->options;
      status = JNI_CreateJavaVM(&self->jvm, (void**) &self->env,
                                &self->vm_args);
      if (status == JNI_ERR) {
          return FALSE;
      }
    }
  return TRUE;
}

static ClassLoader *
java_machine_get_class_loader(JavaVMSingleton *self)
{
  if (self->loader)
    return self->loader;
  JNIEnv *env = NULL;
  (*(self->jvm))->GetEnv(self->jvm, (void **)&env, JNI_VERSION_1_6);
  self->loader = class_loader_new(env);
  g_assert(self->loader);
  return self->loader;
}

void
java_machine_attach_thread(JavaVMSingleton* self, JNIEnv **penv)
{
  g_assert(self == g_jvm_s);
  if ((*(self->jvm))->AttachCurrentThread(self->jvm, (void **)penv, &self->vm_args) == JNI_OK)
    {
      class_loader_init_current_thread(java_machine_get_class_loader(self), *penv);
    }
}

void
java_machine_detach_thread(void)
{
  (*(g_jvm_s->jvm))->DetachCurrentThread(g_jvm_s->jvm);
}


jclass
java_machine_load_class(JavaVMSingleton *self, const gchar *class_name, const gchar *class_path)
{
  JNIEnv *env;
  return class_loader_load_class(java_machine_get_class_loader(self), java_machine_get_env(self, &env), class_name, class_path);
}

JNIEnv *
java_machine_get_env(JavaVMSingleton *self, JNIEnv **penv)
{
  if ((*(self->jvm))->GetEnv(self->jvm, (void **)penv, JNI_VERSION_1_6) != JNI_OK)
    {
      java_machine_attach_thread(self, penv);
    }
  return *penv;
}
