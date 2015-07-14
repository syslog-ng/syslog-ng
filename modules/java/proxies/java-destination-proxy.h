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

#ifndef JAVA_DESTINATION_PROXY_H_
#define JAVA_DESTINATION_PROXY_H_

#include <jni.h>
#include <syslog-ng.h>
#include <template/templates.h>
#include "java_machine.h"


typedef struct _JavaDestinationProxy JavaDestinationProxy;

JavaDestinationProxy *java_destination_proxy_new(const gchar *class_name, const gchar *class_path, gpointer handle, LogTemplate *template);

gboolean java_destination_proxy_init(JavaDestinationProxy *self);
void java_destination_proxy_deinit(JavaDestinationProxy *self);
void java_destination_proxy_on_message_queue_empty(JavaDestinationProxy *self);
gchar *java_destination_proxy_get_name_by_uniq_options(JavaDestinationProxy *self);
gboolean java_destination_proxy_send(JavaDestinationProxy *self, LogMessage *msg);
gboolean java_destination_proxy_open(JavaDestinationProxy *self);
void java_destination_proxy_close(JavaDestinationProxy *self);
gboolean java_destination_proxy_is_opened(JavaDestinationProxy *self);

void java_destination_proxy_set_env(JavaDestinationProxy *self, JNIEnv *env);

void java_destination_proxy_free(JavaDestinationProxy *self);

#endif /* JAVA_DESTINATION_PROXY_H_ */
