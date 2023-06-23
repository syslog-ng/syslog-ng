/*
 * Copyright (c) 2023 Attila Szakacs
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

#include "otel-dest-worker.hpp"

#define get_DestWorker(s) (((OtelDestWorker *) s)->cpp)

using namespace syslogng::grpc::otel;

/* C++ Implementations */

DestWorker::DestWorker(OtelDestWorker *s)
  : super(s)
{
}

bool
DestWorker::init()
{
  return log_threaded_dest_worker_init_method(&super->super);
}

void
DestWorker::deinit()
{
  log_threaded_dest_worker_deinit_method(&super->super);
}

bool
DestWorker::connect()
{
  return true;
}

void
DestWorker::disconnect()
{
}

LogThreadedResult
DestWorker::insert(LogMessage *msg)
{
  return LTR_SUCCESS;
}

LogThreadedResult
DestWorker::flush(LogThreadedFlushMode mode)
{
  return LTR_SUCCESS;
}

/* C Wrappers */

static gboolean
_init(LogThreadedDestWorker *s)
{
  return get_DestWorker(s)->init();
}

static void
_deinit(LogThreadedDestWorker *s)
{
  get_DestWorker(s)->deinit();
}

static gboolean
_connect(LogThreadedDestWorker *s)
{
  return get_DestWorker(s)->connect();
}

static void
_disconnect(LogThreadedDestWorker *s)
{
  get_DestWorker(s)->disconnect();
}

LogThreadedResult
_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  return get_DestWorker(s)->insert(msg);
}

LogThreadedResult
_flush(LogThreadedDestWorker *s, LogThreadedFlushMode mode)
{
  return get_DestWorker(s)->flush(mode);
}

static void
_free(LogThreadedDestWorker *s)
{
  delete get_DestWorker(s);
  log_threaded_dest_worker_free_method(s);
}

LogThreadedDestWorker *
otel_dest_worker_new(LogThreadedDestDriver *o, gint worker_index)
{
  OtelDestWorker *self = g_new0(OtelDestWorker, 1);

  log_threaded_dest_worker_init_instance(&self->super, o, worker_index);

  self->cpp = new DestWorker(self);

  self->super.init = _init;
  self->super.deinit = _deinit;
  self->super.connect = _connect;
  self->super.disconnect = _disconnect;
  self->super.insert = _insert;
  self->super.flush = _flush;
  self->super.free_fn = _free;

  return &self->super;
}
