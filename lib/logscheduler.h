/*
 * Copyright (c) 2022 Balázs Scheidler <bazsi77@gmail.com>
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

#ifndef LOGSCHEDULER_H_INCLUDED
#define LOGSCHEDULER_H_INCLUDED

#include "logpipe.h"
#include "mainloop-io-worker.h"
#include "template/templates.h"

#include <iv_list.h>
#include <iv_event.h>

#define LOGSCHEDULER_MAX_PARTITIONS 16

typedef struct _LogSchedulerBatch
{
  struct iv_list_head elements;
  struct iv_list_head list;
} LogSchedulerBatch;

typedef struct _LogSchedulerPartition
{
  GMutex batches_lock;
  struct iv_list_head batches;
  gboolean flush_running;
  MainLoopIOWorkerJob io_job;
  LogPipe *front_pipe;
} LogSchedulerPartition;

typedef struct _LogSchedulerThreadState
{
  WorkerBatchCallback batch_callback;
  struct iv_list_head batch_by_partition[LOGSCHEDULER_MAX_PARTITIONS];

  guint64 num_messages;
  gint last_partition;
} LogSchedulerThreadState;

typedef struct _LogSchedulerOptions
{
  gint num_partitions;
  LogTemplate *partition_key;
} LogSchedulerOptions;

typedef struct _LogScheduler
{
  LogPipe *front_pipe;
  LogSchedulerOptions *options;
  gint num_threads;
  LogSchedulerPartition partitions[LOGSCHEDULER_MAX_PARTITIONS];
  LogSchedulerThreadState thread_states[];
} LogScheduler;

gboolean log_scheduler_init(LogScheduler *self);
void log_scheduler_deinit(LogScheduler *self);

void log_scheduler_push(LogScheduler *self, LogMessage *msg, const LogPathOptions *path_options);
LogScheduler *log_scheduler_new(LogSchedulerOptions *options, LogPipe *front_pipe);
void log_scheduler_free(LogScheduler *self);

void log_scheduler_options_set_partition_key_ref(LogSchedulerOptions *options, LogTemplate *partition_key);
void log_scheduler_options_defaults(LogSchedulerOptions *options);
gboolean log_scheduler_options_init(LogSchedulerOptions *options, GlobalConfig *cfg);
void log_scheduler_options_destroy(LogSchedulerOptions *options);

#endif
