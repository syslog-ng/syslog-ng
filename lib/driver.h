/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#ifndef DRIVER_H_INCLUDED
#define DRIVER_H_INCLUDED

#include "syslog-ng.h"
#include "logpipe.h"
#include "logqueue.h"
#include "cfg.h"

/*
 * Drivers overview
 * ================
 *
 * In syslog-ng nomenclature a driver is either responsible for handling
 * incoming messages (also known as source driver), or to send them out to
 * another party (also known as the destination driver).  Source drivers are
 * created in "source" statements and destination drivers are similarly
 * created in "destination" statements.
 *
 * Drivers are derived from LogPipes, in essence they use the same "queue"
 * method to forward messages further down the processing pipeline.
 *
 * Driver plugins
 * ==============
 *
 * It is possible to change the behaviour of a driver somewhat by adding
 * "plugins" to drivers.  These plugins basically get a chance to override
 * LogDriver virtual methods, change their semantics and possibly rely on
 * the original behaviour too.  This way, functionalities that are present
 * in all destination drivers can easily be shared, without having to recode
 * the same stuff multiple times.
 *
 * Driver plugins are activated with the "attach" virtual method, which in
 * turn may redirect any of the LogDriver virtual methods to themselves.
 * They can even have a "user_data" pointer, so that they can locate their
 * associated state.
 *
 * Multiple plugins can hook into the same method, by saving the original
 * address & original user_data value.
 *
 */

/* direction agnostic driver class: LogDriver, see specialized source & destination drivers below */

typedef struct _LogDriver LogDriver;
typedef struct _LogDriverPlugin LogDriverPlugin;

struct _LogDriverPlugin
{
  const gchar *name;
  /* this function is called when the plugin is attached to a LogDriver
   * instance.  It should do whatever it is necessary to extend the
   * functionality of the driver specified (e.g.  hook into various
   * methods).
   */

  gboolean (*attach)(LogDriverPlugin *s, LogDriver *d);
  void (*detach)(LogDriverPlugin *s, LogDriver *d);
  void (*free_fn)(LogDriverPlugin *s);
};

static inline gboolean
log_driver_plugin_attach(LogDriverPlugin *self, LogDriver *d)
{
  return self->attach(self, d);
}

static inline void
log_driver_plugin_detach(LogDriverPlugin *self, LogDriver *d)
{
  if (self->detach)
    self->detach(self, d);
}

static inline void
log_driver_plugin_free(LogDriverPlugin *self)
{
  self->free_fn(self);
}

void log_driver_plugin_init_instance(LogDriverPlugin *self, const gchar *name);
void log_driver_plugin_free_method(LogDriverPlugin *self);

struct _LogDriver
{
  LogPipe super;

  gboolean optional;
  gchar *group;
  gchar *id;
  GList *plugins;

  StatsCounterItem *processed_group_messages;

};

gboolean log_driver_add_plugin(LogDriver *self, LogDriverPlugin *plugin);
void log_driver_append(LogDriver *self, LogDriver *next);
LogDriverPlugin *log_driver_lookup_plugin(LogDriver *self, const gchar *name);

#define log_driver_get_plugin(self, T, name) \
  ({ \
    T *plugin = (T *) log_driver_lookup_plugin(self, name); \
    g_assert(plugin != NULL); \
    plugin; \
  })

/* methods registered to the init/deinit virtual functions */
gboolean log_driver_init_method(LogPipe *s);
gboolean log_driver_deinit_method(LogPipe *s);


/* source driver class: LogSourceDriver */

typedef struct _LogSrcDriver LogSrcDriver;

struct _LogSrcDriver
{
  LogDriver super;
  gint group_len;
  StatsCounterItem *received_global_messages;
};

gboolean log_src_driver_init_method(LogPipe *s);
gboolean log_src_driver_deinit_method(LogPipe *s);
void log_src_driver_queue_method(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options);
void log_src_driver_init_instance(LogSrcDriver *self, GlobalConfig *cfg);
void log_src_driver_free(LogPipe *s);

/* destination driver class: LogDestDriver */

typedef struct _LogDestDriver LogDestDriver;

struct _LogDestDriver
{
  LogDriver super;

  LogQueue *(*acquire_queue)(LogDestDriver *s, const gchar *persist_name);
  void (*release_queue)(LogDestDriver *s, LogQueue *q);

  /* queues managed by this LogDestDriver, all constructed queues come
   * here and are automatically saved into cfg_persist & persist_state. */
  GList *queues;

  gint log_fifo_size;
  gint throttle;
  StatsCounterItem *queued_global_messages;
};

/* returns a reference */
static inline LogQueue *
log_dest_driver_acquire_queue(LogDestDriver *self, const gchar *persist_name)
{
  LogQueue *q;

  q = self->acquire_queue(self, persist_name);
  if (q)
    {
      self->queues = g_list_prepend(self->queues, q);
    }
  return q;
}

/* consumes the reference in @q */
static inline void
log_dest_driver_release_queue(LogDestDriver *self, LogQueue *q)
{
  if (q)
    {
      self->queues = g_list_remove(self->queues, q);

      /* this drops the reference passed by the caller */
      self->release_queue(self, q);
      /* this drops the reference stored on the list */
      log_queue_unref(q);
    }
}

gboolean log_dest_driver_init_method(LogPipe *s);
gboolean log_dest_driver_deinit_method(LogPipe *s);
void log_dest_driver_queue_method(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options);

void log_dest_driver_init_instance(LogDestDriver *self, GlobalConfig *cfg);
void log_dest_driver_free(LogPipe *s);


#endif
