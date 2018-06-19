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

#include "redisq-options.h"
#include "syslog-ng.h"
#include "messages.h"
#include "reloc.h"

void
redis_queue_options_host_set(RedisQueueOptions *self, gchar *host)
{
  if (self->host)
    {
      g_free(self->host);
    }

  self->host = g_strdup(host);
}

void
redis_queue_options_port_set(RedisQueueOptions *self, gint port)
{
  self->port = port;
}

void
redis_queue_options_auth_set(RedisQueueOptions *self, gchar *auth)
{
  if (self->auth)
    {
      g_free(self->auth);
    }

  self->auth = g_strdup(auth);
}

void
redis_queue_options_key_prefix_set(RedisQueueOptions *self, gchar *keyprefix)
{
  if (self->keyprefix)
    {
      g_free(self->keyprefix);
    }

  self->keyprefix = g_strdup(keyprefix);
}

void
redis_queue_options_check_plugin_settings(RedisQueueOptions *self)
{

}

void
redis_queue_options_set_default_options(RedisQueueOptions *self)
{
  self->host = g_strdup("127.0.0.1");
  self->port = 6379;
  self->auth = NULL;
  self->keyprefix = g_strdup("syslogng_redisq");
}

void
redis_queue_options_destroy(RedisQueueOptions *self)
{
  if (self->host)
    {
      g_free(self->host);
      self->host = NULL;
    }

  if (self->auth)
    {
      g_free(self->auth);
      self->auth = NULL;
    }

  if (self->keyprefix)
    {
      g_free(self->keyprefix);
      self->keyprefix = NULL;
    }
}
