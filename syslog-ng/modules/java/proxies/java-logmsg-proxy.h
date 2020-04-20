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


#ifndef JAVA_LOGMSG_PROXY_H_
#define JAVA_LOGMSG_PROXY_H_

#include "org_syslog_ng_LogMessage.h"
#include "logmsg/logmsg.h"

typedef struct _JavaLogMessageProxy JavaLogMessageProxy;

JavaLogMessageProxy *java_log_message_proxy_new(void);
void java_log_message_proxy_free(JavaLogMessageProxy *self);

jobject java_log_message_proxy_create_java_object(JavaLogMessageProxy *self, LogMessage *msg);

#endif /* JAVA_LOGMSG_PROXY_H_ */
