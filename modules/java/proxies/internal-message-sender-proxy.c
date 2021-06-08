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

#include "messages.h"
#include "org_syslog_ng_InternalMessageSender.h"

JNIEXPORT void JNICALL
Java_org_syslog_1ng_InternalMessageSender_createInternalMessage(JNIEnv *env, jclass cls, jint pri, jstring message)
{
  if ((pri != org_syslog_ng_InternalMessageSender_LevelDebug) || debug_flag)
    {
      const char *c_str = (*env)->GetStringUTFChars(env, message, 0);
      msg_event_suppress_recursions_and_send(msg_event_create(pri, c_str, NULL));

      (*env)->ReleaseStringUTFChars(env, message, c_str);
    }
}

JNIEXPORT jint JNICALL
Java_org_syslog_1ng_InternalMessageSender_getLevel(JNIEnv *env, jclass cls)
{
  if (trace_flag || debug_flag)
    return org_syslog_ng_InternalMessageSender_LevelDebug;

  return org_syslog_ng_InternalMessageSender_LevelInfo;
}
