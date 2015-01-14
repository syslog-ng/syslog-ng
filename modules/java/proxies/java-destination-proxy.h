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

JavaDestinationProxy *java_destination_proxy_new(const gchar *class_name, const gchar *class_path, gpointer impl, LogTemplate *template);

gboolean java_destination_proxy_init(JavaDestinationProxy *self, JNIEnv *env, void *ptr);
void java_destination_proxy_deinit(JavaDestinationProxy *self, JNIEnv *env);
gboolean java_destination_proxy_flush(JavaDestinationProxy *self, JNIEnv *env);
gboolean java_destination_proxy_queue(JavaDestinationProxy *self, JNIEnv *env, LogMessage *msg);

void java_destination_proxy_free(JavaDestinationProxy *self);

#endif /* JAVA_DESTINATION_PROXY_H_ */
