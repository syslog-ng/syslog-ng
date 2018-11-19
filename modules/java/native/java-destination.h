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

#ifndef MODJAVA_H_INCLUDED
#define MODJAVA_H_INCLUDED

#include <jni.h>
#include <iv.h>
#include <iv_event.h>
#include "driver.h"
#include "logthrdestdrv.h"
#include "logqueue.h"
#include "mainloop.h"
#include "mainloop-io-worker.h"
#include "java_machine.h"
#include "proxies/java-destination-proxy.h"



typedef struct
{
  LogThreadedDestDriver super;
  JavaDestinationProxy *proxy;
  GString *class_path;
  gchar *class_name;
  LogTemplate *template;
  gchar *template_string;
  GHashTable *options;
  LogTemplateOptions template_options;
} JavaDestDriver;

LogDriver *java_dd_new(GlobalConfig *cfg);
void java_dd_set_class_path(LogDriver *s, const gchar *class_path);
void java_dd_set_class_name(LogDriver *s, const gchar *class_name);
void java_dd_set_template_string(LogDriver *s, const gchar *template_string);
void java_dd_set_retries(LogDriver *s, guint retries);
void java_dd_set_option(LogDriver *s, const gchar *key, const gchar *value);
LogTemplateOptions *java_dd_get_template_options(LogDriver *s);

#endif
