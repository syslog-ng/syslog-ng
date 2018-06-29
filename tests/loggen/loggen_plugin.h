/*
 * Copyright (c) 2018 Balabit
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

#ifndef LOGGEN_PLUGIN_H_INCLUDED
#define LOGGEN_PLUGIN_H_INCLUDED

#include "compat/glib.h"
#include <gmodule.h>
#include <sys/time.h>

typedef struct _plugin_option
{
  int message_length;
  int interval;
  int number_of_messages;
  int permanent;
  int active_connections;
  int idle_connections;
  int use_ipv6;
  const char *target; /* command line argument */
  const char *port;
  int  rate;
} PluginOption;

typedef struct _thread_data
{
  PluginOption *option;
  int index;
  int sent_messages;
  struct timeval start_time;
  struct timeval last_throttle_check;
  long buckets;
} ThreadData;

typedef GOptionEntry *(*get_option_func)(void);
typedef void (*start_plugin_func)(PluginOption *option);
typedef void (*stop_plugin_func)(PluginOption *option);
typedef int (*generate_message_func)(char *buffer, int buffer_size, int thread_id, unsigned long seq);
typedef void (*set_generate_message_func)(generate_message_func gen_message);
typedef int (*get_thread_count_func)(void);
typedef gboolean (*is_plugin_activated_func)(void);
#define LOGGEN_PLUGIN_INFO "loggen_plugin_info"

typedef struct _plugin_info
{
  gchar              *name;
  get_option_func    get_options_list;
  get_thread_count_func  get_thread_count;
  stop_plugin_func   stop_plugin;
  start_plugin_func  start_plugin;
  set_generate_message_func set_generate_message;
  gboolean  require_framing; /* plugin can indicates that framing is mandatory in message lines */
  is_plugin_activated_func is_plugin_activated;
} PluginInfo;

gboolean thread_check_exit_criteria(ThreadData *thread_context);
gboolean thread_check_time_bucket(ThreadData *thread_context);

#endif
