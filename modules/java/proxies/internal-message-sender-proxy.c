/*
 * Copyright (c) 2010-2015 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2010-2015 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "messages.h"
#include "InternalMessageSender.h"

JNIEXPORT void JNICALL
Java_org_syslog_1ng_InternalMessageSender_createInternalMessage(JNIEnv *env, jclass cls, jint pri, jstring message)
{
  if ((pri != org_syslog_ng_InternalMessageSender_MsgDebug) || debug_flag)
    {
      const char *c_str = (*env)->GetStringUTFChars(env, message, 0);
      if (msg_limit_internal_message())
        {
          msg_event_send(msg_event_create(pri, c_str, NULL));
        }
      (*env)->ReleaseStringUTFChars(env, message, c_str);
    }
}
