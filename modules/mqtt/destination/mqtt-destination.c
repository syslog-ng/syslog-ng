/*
 * Copyright (c) 2021 One Identity
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

#include "mqtt-destination.h"
#include "mqtt-worker.h"

#include "plugin.h"
#include "messages.h"
#include "misc.h"
#include "stats/stats-registry.h"
#include "logqueue.h"
#include "driver.h"
#include "plugin-types.h"
#include "logthrdest/logthrdestdrv.h"

/*
 * Default values
 */

#define DEFAULT_ADDRESS "tcp://localhost:1883"
#define DEFAULT_KEEPALIVE 60


/*
 * Configuration
 */

void
mqtt_dd_set_topic(LogDriver *d, const gchar *topic)
{
  MQTTDestinationDriver *self = (MQTTDestinationDriver *)d;
  g_string_assign(self->topic, topic);
}


void
mqtt_dd_set_keepalive (LogDriver *d, const gint keepalive)
{
  MQTTDestinationDriver *self = (MQTTDestinationDriver *)d;
  self->keepalive = keepalive;
}

void
mqtt_dd_set_address(LogDriver *d, const gchar *address)
{
  MQTTDestinationDriver *self = (MQTTDestinationDriver *)d;

  g_string_assign(self->address, address);
}

/*
 * Utilities
 */
static const gchar *
_format_stats_instance(LogThreadedDestDriver *d)
{
  MQTTDestinationDriver *self = (MQTTDestinationDriver *)d;
  static gchar stats_instance[1024];

  if (((LogPipe *)d)->persist_name)
    g_snprintf(stats_instance, sizeof(stats_instance), "%s", ((LogPipe *)d)->persist_name);
  else
    g_snprintf(stats_instance, sizeof(stats_instance), "mqtt,%s,%s", self->address->str,
               self->topic_name->template);

  return stats_instance;
}

static const gchar *
_format_persist_name(const LogPipe *d)
{
  MQTTDestinationDriver *self = (MQTTDestinationDriver *)d;
  static gchar persist_name[1024];

  if (d->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "mqtt-destination.%s", d->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "mqtt-destination.(%s,%s)", self->address->str,
               self->topic_name->template);

  return persist_name;
}

static void
_set_default_value(MQTTDestinationDriver *self, GlobalConfig *cfg)
{
  self->address       = g_string_new(DEFAULT_ADDRESS);

  self->keepalive     = DEFAULT_KEEPALIVE;
}

static gboolean
_init(LogPipe *d)
{
  MQTTDestinationDriver *self = (MQTTDestinationDriver *)d;

  if (self->topic->len == 0)
    {
      msg_error("ERROR, topic must be set!"); 
      return FALSE;
    }

  if (self->super.batch_lines != -1 || self->super.batch_timeout != -1)
    {
      msg_error("The mqtt destination does not support the batching of messages, so none of the batching related parameters can be set (batch-timeout, batch-lines)",
                evt_tag_str("driver", self->super.super.super.id),
                log_pipe_location_tag(&self->super.super.super.super));
      return FALSE;
    }

  if (!log_threaded_dest_driver_init_method(d))
    {
      return FALSE;
    }

  return TRUE;
}

static void
_free(LogPipe *d)
{
  MQTTDestinationDriver *self = (MQTTDestinationDriver *)d;

  g_string_free(self->topic, TRUE);
  g_string_free(self->address, TRUE);

  log_threaded_dest_driver_free(d);
}

LogDriver *
mqtt_dd_new(GlobalConfig *cfg)
{
  MQTTDestinationDriver *self = g_new0(MQTTDestinationDriver, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);

  _set_default_value(self, cfg);

  self->super.super.super.super.init = _init;
  self->super.super.super.super.free_fn = _free;

  self->super.format_stats_instance = _format_stats_instance;
  self->super.super.super.super.generate_persist_name = _format_persist_name;
  self->super.stats_source = stats_register_type("mqtt-destination");
  self->super.worker.construct = mqtt_dw_new;

  return (LogDriver *)self;
}

static gboolean
_validate_protocol(const gchar *address)
{
  const gchar *valid_type[] = {"tcp", "ssl", "ws", "wss"};
  gint i;

  for (i = 0; i < G_N_ELEMENTS(valid_type); ++i)
    if (strncmp(valid_type[i], address, strlen(valid_type[i])) == 0)
      return TRUE;

  return FALSE;
}

gboolean
mqtt_dd_validate_address(const gchar *address)
{
  if (strstr(address, "://") == NULL)
    return FALSE;

  if (!_validate_protocol(address))
    return FALSE;

  return TRUE;
}

