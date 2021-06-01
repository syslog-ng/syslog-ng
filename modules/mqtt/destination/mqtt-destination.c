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
 * Configuration
 */


void
mqtt_dd_set_topic(LogDriver *d, const gchar *topic)
{
  MQTTDestinationDriver *self = (MQTTDestinationDriver *)d;
  // TODO
}


void
mqtt_dd_set_keepalive (LogDriver *d, const gint keepalive)
{
  MQTTDestinationDriver *self = (MQTTDestinationDriver *)d;
  // TODO
}

/*
 * Utilities
 */
static const gchar *
_format_stats_instance(LogThreadedDestDriver *d)
{
  MQTTDestinationDriver *self = (MQTTDestinationDriver *)d;
  static gchar stats_instance[1024];

  // TODO

  return stats_instance;
}

static const gchar *
_format_persist_name(const LogPipe *d)
{
  MQTTDestinationDriver *self = (MQTTDestinationDriver *)d;
  static gchar persist_name[1024];

  // TODO

  return persist_name;
}

static void
_set_default_value(MQTTDestinationDriver *self)
{
  // TODO
}

static gboolean
_init(LogPipe *d)
{
  MQTTDestinationDriver *self = (MQTTDestinationDriver *)d;

  // TODO

  return TRUE;
}

static void
_free(LogPipe *d)
{
  MQTTDestinationDriver *self = (MQTTDestinationDriver *)d;

  // TODO

  log_threaded_dest_driver_free(d);
}

LogDriver *
mqtt_dd_new(GlobalConfig *cfg)
{
  MQTTDestinationDriver *self = g_new0(MQTTDestinationDriver, 1);

  _set_default_value(self);
  // TODO

  return (LogDriver *)self;
}
