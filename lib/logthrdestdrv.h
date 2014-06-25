/*
 * Copyright (c) 2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2013 Gergely Nagy <algernon@balabit.hu>
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

#ifndef LOGTHRDESTDRV_H
#define LOGTHRDESTDRV_H

#include "syslog-ng.h"
#include "driver.h"
#include "stats/stats-registry.h"
#include "logqueue.h"
#include "mainloop-worker.h"

typedef struct _LogThrDestDriver LogThrDestDriver;

struct _LogThrDestDriver
{
  LogDestDriver super;

  StatsCounterItem *dropped_messages;
  StatsCounterItem *stored_messages;

  time_t time_reopen;

  /* Thread related stuff; shared */
  GMutex *suspend_mutex;
  GCond *writer_thread_wakeup_cond;

  gboolean writer_thread_terminate;
  gboolean writer_thread_suspended;
  GTimeVal writer_thread_suspend_target;

  LogQueue *queue;

  /* Worker stuff */
  struct
  {
    void (*thread_init) (LogThrDestDriver *s);
    void (*thread_deinit) (LogThrDestDriver *s);
    gboolean (*insert) (LogThrDestDriver *s);
    void (*disconnect) (LogThrDestDriver *s);
  } worker;

  struct
  {
    gchar *(*stats_instance) (LogThrDestDriver *s);
    gchar *(*persist_name) (LogThrDestDriver *s);
  } format;
  gint stats_source;

  void (*queue_method) (LogThrDestDriver *s);
  WorkerOptions worker_options;
};

gboolean log_threaded_dest_driver_deinit_method(LogPipe *s);
gboolean log_threaded_dest_driver_start(LogPipe *s);

void log_threaded_dest_driver_init_instance(LogThrDestDriver *self, GlobalConfig *cfg);
void log_threaded_dest_driver_free(LogPipe *s);

void log_threaded_dest_driver_suspend(LogThrDestDriver *self);

#endif
