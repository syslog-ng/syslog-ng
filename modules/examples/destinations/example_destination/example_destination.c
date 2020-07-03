/*
 * Copyright (c) 2020 Balabit
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

#include "example_destination.h"
#include "example_destination-parser.h"

#include "plugin.h"
#include "messages.h"
#include "misc.h"
#include "stats/stats-registry.h"
#include "logqueue.h"
#include "driver.h"
#include "plugin-types.h"
#include "logthrdest/logthrdestdrv.h"


typedef struct
{
  LogThreadedDestDriver super;
  gchar *filename;
} ExampleDestination;

/*
 * Configuration
 */

void
example_destination_dd_set_filename(LogDriver *d, const gchar *filename)
{
  ExampleDestination *self = (ExampleDestination *)d;

  g_free(self->filename);
  self->filename = g_strdup(filename);
}

/*
 * Utilities
 */

static const gchar *
_format_stats_instance(LogThreadedDestDriver *d)
{
  ExampleDestination *self = (ExampleDestination *)d;
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
             "example-destination,%s", self->filename);
  return persist_name;
}

static const gchar *
_format_persist_name(const LogPipe *d)
{
  ExampleDestination *self = (ExampleDestination *)d;
  static gchar persist_name[1024];

  if (d->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "example-destination.%s", d->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "example-destination.%s", self->filename);

  return persist_name;
}

static gboolean
_connect(LogThreadedDestDriver *s)
{
  msg_debug("Connection succeeded",
            evt_tag_str("driver", s->super.super.id), NULL);

  return TRUE;
}

static void
_disconnect(LogThreadedDestDriver *d)
{
  ExampleDestination *self = (ExampleDestination *)d;

  msg_debug("Connection closed",
            evt_tag_str("driver", self->super.super.super.id), NULL);
}

/*
 * Worker thread
 */

static LogThreadedResult
_insert(LogThreadedDestDriver *d, LogMessage *msg)
{
  ExampleDestination *self = (ExampleDestination *)d;

  fprintf(stderr, "Message sent using file name: %s\n", self->filename);

  return LTR_SUCCESS;
  /*
   * LTR_DROP,
   * LTR_ERROR,
   * LTR_SUCCESS,
   * LTR_QUEUED,
   * LTR_NOT_CONNECTED,
   * LTR_RETRY,
  */
}

static void
_thread_init(LogThreadedDestDriver *d)
{
  ExampleDestination *self = (ExampleDestination *)d;

  msg_debug("Worker thread started",
            evt_tag_str("driver", self->super.super.super.id),
            NULL);
}

static void
_thread_deinit(LogThreadedDestDriver *d)
{
  ExampleDestination *self = (ExampleDestination *)d;

  msg_debug("Worker thread stopped",
            evt_tag_str("driver", self->super.super.super.id),
            NULL);
}

/*
 * Main thread
 */

static gboolean
_dd_init(LogPipe *d)
{
  ExampleDestination *self = (ExampleDestination *)d;

  if (!log_threaded_dest_driver_init_method(d))
    return FALSE;

  msg_verbose("Initializing ExampleDestination",
              evt_tag_str("driver", self->super.super.super.id),
              evt_tag_str("filename", self->filename),
              NULL);

  return TRUE;
}


gboolean
_dd_deinit(LogPipe *s)
{
  ExampleDestination *self = (ExampleDestination *)s;

  msg_verbose("Deinitializing ExampleDestination",
              evt_tag_str("driver", self->super.super.super.id),
              evt_tag_str("filename", self->filename),
              NULL);

  return log_threaded_dest_driver_deinit_method(s);
}

static void
_dd_free(LogPipe *d)
{
  ExampleDestination *self = (ExampleDestination *)d;

  g_free(self->filename);

  log_threaded_dest_driver_free(d);
}

/*
 * Plugin glue.
 */

LogDriver *
example_destination_dd_new(GlobalConfig *cfg)
{
  ExampleDestination *self = g_new0(ExampleDestination, 1);

  log_threaded_dest_driver_init_instance(&self->super, cfg);
  self->super.super.super.super.init = _dd_init;
  self->super.super.super.super.deinit = _dd_deinit;
  self->super.super.super.super.free_fn = _dd_free;

  self->super.worker.thread_init = _thread_init;
  self->super.worker.thread_deinit = _thread_deinit;
  self->super.worker.connect = _connect;
  self->super.worker.disconnect = _disconnect;
  self->super.worker.insert = _insert;

  self->super.format_stats_instance = _format_stats_instance;
  self->super.super.super.super.generate_persist_name = _format_persist_name;
  self->super.stats_source = stats_register_type("example-destination");

  return (LogDriver *)self;
}
