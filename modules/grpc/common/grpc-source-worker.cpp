/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2023-2024 Attila Szakacs <attila.szakacs@axoflow.com>
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

#include "grpc-source-worker.hpp"

using namespace syslogng::grpc;

/* C++ Implementations */

SourceWorker::SourceWorker(GrpcSourceWorker *s, SourceDriver &d)
  : super(s), driver(d)
{
}

void
SourceWorker::post(LogMessage *msg)
{
  log_threaded_source_worker_blocking_post(&super->super, msg);
}

/* C Wrappers */

static void
_free(LogPipe *s)
{
  GrpcSourceWorker *self = (GrpcSourceWorker *) s;
  delete self->cpp;
  log_threaded_source_worker_free(s);
}

static void
_run(LogThreadedSourceWorker *s)
{
  GrpcSourceWorker *self = (GrpcSourceWorker *) s;
  self->cpp->run();
}

static void
_request_exit(LogThreadedSourceWorker *s)
{
  GrpcSourceWorker *self = (GrpcSourceWorker *) s;
  self->cpp->request_exit();
}

GrpcSourceWorker *
grpc_sw_new(GrpcSourceDriver *o, gint worker_index)
{
  GrpcSourceWorker *self = g_new0(GrpcSourceWorker, 1);

  log_threaded_source_worker_init_instance(&self->super, &o->super, worker_index);
  self->super.run = _run;
  self->super.request_exit = _request_exit;
  self->super.super.super.free_fn = _free;

  return self;
}
