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


#include "java-template-proxy.h"
#include "messages.h"
#include "cfg.h"
#include "cfg-tree.h"

JNIEXPORT jboolean JNICALL
Java_org_syslog_1ng_options_TemplateOption_isNamedTemplate(JNIEnv *env, jobject obj, jlong cfg_handle,
                                                           jstring template_name)
{
  GlobalConfig *cfg = (GlobalConfig *)cfg_handle;
  const char *template_cstr = (*env)->GetStringUTFChars(env, template_name, NULL);
  jboolean result = !!cfg_tree_lookup_template(&cfg->tree, template_cstr);
  (*env)->ReleaseStringUTFChars(env, template_name, template_cstr);
  return result;
}


JNIEXPORT jlong JNICALL
Java_org_syslog_1ng_LogTemplate_create_1new_1template_1instance(JNIEnv *env, jobject obj, jlong cfg_handle)
{
  GlobalConfig *cfg = (GlobalConfig *)cfg_handle;
  LogTemplate *template = log_template_new(cfg, NULL);
  return (jlong)template;
}


JNIEXPORT jboolean
JNICALL Java_org_syslog_1ng_LogTemplate_compile(JNIEnv *env, jobject obj, jlong template_handle,
                                                jstring template_string)
{
  LogTemplate *template = (LogTemplate *)template_handle;
  GError *error = NULL;
  const char *template_cstr = (*env)->GetStringUTFChars(env, template_string, NULL);
  gboolean result = log_template_compile(template, template_cstr, &error);
  if (!result)
    {
      msg_error("Can't compile template",
                evt_tag_str("template", template_cstr),
                evt_tag_str("error", error->message));
    }
  (*env)->ReleaseStringUTFChars(env, template_string, template_cstr);
  return result;
}

JNIEXPORT jstring JNICALL
Java_org_syslog_1ng_LogTemplate_format(JNIEnv *env, jobject obj, jlong template_handle, jlong msg_handle,
                                       jlong logtemplate_options_handle, jint tz, jint seqnum)
{
  LogTemplate *template = (LogTemplate *)template_handle;
  LogTemplateOptions *template_options = (LogTemplateOptions *)logtemplate_options_handle;
  LogMessage *msg = (LogMessage *)msg_handle;
  GString *formatted_message = g_string_sized_new(1024);
  jstring result = NULL;
  log_template_format(template, msg, template_options, tz, seqnum, NULL, formatted_message);
  result = (*env)->NewStringUTF(env, formatted_message->str);
  g_string_free(formatted_message, TRUE);
  return result;
}

JNIEXPORT void JNICALL
Java_org_syslog_1ng_LogTemplate_unref(JNIEnv *env, jobject obj, jlong template_handle)
{
  LogTemplate *template = (LogTemplate *)template_handle;
  log_template_unref(template);
}
