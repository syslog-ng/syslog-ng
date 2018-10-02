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

#ifndef LOGTHRFETCHERDRV_H
#define LOGTHRFETCHERDRV_H

#include "syslog-ng.h"
#include "logthrsourcedrv.h"
#include "logmsg/logmsg.h"
#include "compat/time.h"

#include <iv.h>
#include <iv_event.h>

typedef struct _LogThreadedFetcherDriver LogThreadedFetcherDriver;

typedef enum
{
  THREADED_FETCH_ERROR,
  THREADED_FETCH_NOT_CONNECTED,
  THREADED_FETCH_SUCCESS
} ThreadedFetchResult;

typedef struct _LogThreadedFetchResult
{
  ThreadedFetchResult result;
  LogMessage *msg;
} LogThreadedFetchResult;

struct _LogThreadedFetcherDriver
{
  LogThreadedSourceDriver super;
  time_t time_reopen;
  struct iv_task fetch_task;
  struct iv_event wakeup_event;
  struct iv_event shutdown_event;
  struct iv_timer reconnect_timer;
  gboolean suspended;
  gboolean under_termination;

  void (*thread_init)(LogThreadedFetcherDriver *self);
  void (*thread_deinit)(LogThreadedFetcherDriver *self);
  gboolean (*connect)(LogThreadedFetcherDriver *self);
  void (*disconnect)(LogThreadedFetcherDriver *self);
  LogThreadedFetchResult (*fetch)(LogThreadedFetcherDriver *self);

  void (*request_exit)(LogThreadedFetcherDriver *self);
};

void log_threaded_fetcher_driver_init_instance(LogThreadedFetcherDriver *self, GlobalConfig *cfg);
gboolean log_threaded_fetcher_driver_init_method(LogPipe *s);
gboolean log_threaded_fetcher_driver_deinit_method(LogPipe *s);
void log_threaded_fetcher_driver_free_method(LogPipe *s);

#endif
