/*
 * Copyright (c) 2023 László Várady
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

#include "bigquery-worker.hpp"

#include "compat/cpp-start.h"
#include "logthrdest/logthrdestdrv.h"
#include "compat/cpp-end.h"

using syslog_ng::bigquery::DestinationWorker;

struct _BigQueryDestWorker
{
  LogThreadedDestWorker super;
  DestinationWorker *cpp;
};


DestinationWorker::DestinationWorker(BigQueryDestWorker *s) : super(s)
{

}

DestinationWorker::~DestinationWorker()
{

}

bool
DestinationWorker::init()
{
  return log_threaded_dest_worker_init_method(&this->super->super);
}

void
DestinationWorker::deinit()
{
  log_threaded_dest_worker_deinit_method(&this->super->super);
}

LogThreadedResult
DestinationWorker::insert(LogMessage *msg)
{
  return LTR_QUEUED;
}

LogThreadedResult
DestinationWorker::flush(LogThreadedFlushMode mode)
{
  return LTR_SUCCESS;
}

/* C Wrappers */

static LogThreadedResult
_insert(LogThreadedDestWorker *s, LogMessage *msg)
{
  BigQueryDestWorker *self = (BigQueryDestWorker *) s;
  return self->cpp->insert(msg);
}

static LogThreadedResult
_flush(LogThreadedDestWorker *s, LogThreadedFlushMode mode)
{
  BigQueryDestWorker *self = (BigQueryDestWorker *) s;
  return self->cpp->flush(mode);
}

static gboolean
_init(LogThreadedDestWorker *s)
{
  BigQueryDestWorker *self = (BigQueryDestWorker *) s;
  return self->cpp->init();
}

static void
_deinit(LogThreadedDestWorker *s)
{
  BigQueryDestWorker *self = (BigQueryDestWorker *) s;
  self->cpp->deinit();
}

static void
_free(LogThreadedDestWorker *s)
{
  BigQueryDestWorker *self = (BigQueryDestWorker *) s;
  delete self->cpp;

  log_threaded_dest_worker_free_method(s);
}

LogThreadedDestWorker *
bigquery_dw_new(LogThreadedDestDriver *o, gint worker_index)
{
  BigQueryDestWorker *self = g_new0(BigQueryDestWorker, 1);

  log_threaded_dest_worker_init_instance(&self->super, o, worker_index);

  self->cpp = new DestinationWorker(self);

  self->super.init = _init;
  self->super.deinit = _deinit;
  self->super.insert = _insert;
  self->super.flush = _flush;
  self->super.free_fn = _free;

  return &self->super;
}
