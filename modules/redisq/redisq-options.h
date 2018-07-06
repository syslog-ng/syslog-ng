/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 Mehul Prajapati <mehulprajapati2802@gmail.com>
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

#ifndef REDISQ_OPTIONS_H_
#define REDISQ_OPTIONS_H_

#include "syslog-ng.h"


typedef struct _RedisQueueOptions
{
  gchar *host;
  gint port;
  gchar *auth;
  gchar *keyprefix;
  gint conn_timeout;
} RedisQueueOptions;

void redis_queue_options_host_set(RedisQueueOptions *self, gchar *host);
void redis_queue_options_port_set(RedisQueueOptions *self, gint port);
void redis_queue_options_auth_set(RedisQueueOptions *self, gchar *auth);
void redis_queue_options_key_prefix_set(RedisQueueOptions *self, gchar *keyprefix);
void redis_queue_options_conn_timeout_set(RedisQueueOptions *self, gint conn_timeout);
void redis_queue_options_check_plugin_settings(RedisQueueOptions *self);
void redis_queue_options_set_default_options(RedisQueueOptions *self);
void redis_queue_options_destroy(RedisQueueOptions *self);

#endif /* REDISQ_OPTIONS_H_ */
