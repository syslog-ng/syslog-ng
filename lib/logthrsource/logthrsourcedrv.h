/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 László Várady <laszlo.varady@balabit.com>
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

#ifndef LOGTHRSOURCEDRV_H
#define LOGTHRSOURCEDRV_H

#include "syslog-ng.h"
#include "driver.h"
#include "logsource.h"
#include "cfg.h"
#include "logpipe.h"
#include "logmsg/logmsg.h"

typedef struct _LogThreadedSourceDriver LogThreadedSourceDriver;
typedef struct _LogThreadedSourceWorker LogThreadedSourceWorker;

typedef void (*LogThreadedSourceWorkerRunFunc)(LogThreadedSourceDriver *);
typedef void (*LogThreadedSourceWorkerRequestExitFunc)(LogThreadedSourceDriver *);
typedef void (*LogThreadedSourceWorkerWakeupFunc)(LogThreadedSourceDriver *);

typedef struct _LogThreadedSourceWorkerOptions
{
  LogSourceOptions super;
} LogThreadedSourceWorkerOptions;

struct _LogThreadedSourceDriver
{
  LogSrcDriver super;
  LogThreadedSourceWorkerOptions worker_options;
  LogThreadedSourceWorker *worker;

  const gchar *(*format_stats_instance)(LogThreadedSourceDriver *self);
};

void log_threaded_source_worker_options_defaults(LogThreadedSourceWorkerOptions *options);
void log_threaded_source_worker_options_init(LogThreadedSourceWorkerOptions *options, GlobalConfig *cfg,
                                             const gchar *group_name);
void log_threaded_source_worker_options_destroy(LogThreadedSourceWorkerOptions *options);

void log_threaded_source_driver_init_instance(LogThreadedSourceDriver *self, GlobalConfig *cfg);
gboolean log_threaded_source_driver_init_method(LogPipe *s);
gboolean log_threaded_source_driver_deinit_method(LogPipe *s);
void log_threaded_source_driver_free_method(LogPipe *s);

void log_threaded_source_driver_set_worker_run_func(LogThreadedSourceDriver *self, LogThreadedSourceWorkerRunFunc run);
void log_threaded_source_driver_set_worker_request_exit_func(LogThreadedSourceDriver *self,
    LogThreadedSourceWorkerRequestExitFunc request_exit);

static inline LogSourceOptions *
log_threaded_source_driver_get_source_options(LogDriver *s)
{
  LogThreadedSourceDriver *self = (LogThreadedSourceDriver *) s;

  return &self->worker_options.super;
}

/* blocking API */
void log_threaded_source_blocking_post(LogThreadedSourceDriver *self, LogMessage *msg);

/* non-blocking API, use it wisely (thread boundaries) */
void log_threaded_source_set_wakeup_func(LogThreadedSourceDriver *self, LogThreadedSourceWorkerWakeupFunc wakeup);
void log_threaded_source_post(LogThreadedSourceDriver *self, LogMessage *msg);
gboolean log_threaded_source_free_to_send(LogThreadedSourceDriver *self);

#endif
