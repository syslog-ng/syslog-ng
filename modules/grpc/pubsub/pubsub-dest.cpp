/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
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

#include "pubsub-dest.hpp"
#include "pubsub-dest-worker.hpp"
#include "grpc-dest-worker.hpp"

#include "compat/cpp-start.h"
#include "messages.h"
#include "compat/cpp-end.h"

using syslogng::grpc::pubsub::DestDriver;

DestDriver::DestDriver(GrpcDestDriver *s)
  : syslogng::grpc::DestDriver(s)
{
}

bool
DestDriver::init()
{
  return syslogng::grpc::DestDriver::init();
}

const gchar *
DestDriver::generate_persist_name()
{
  static gchar persist_name[1024];

  // TODO: update when options are available

  LogPipe *s = &this->super->super.super.super.super;
  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "google_pubsub_grpc.%s", s->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "google_pubsub_grpc(%s)", this->url.c_str());

  return persist_name;
}

const gchar *
DestDriver::format_stats_key(StatsClusterKeyBuilder *kb)
{
  // TODO: update when options are available

  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "pubsub"));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("url", this->url.c_str()));

  return nullptr;
}

LogThreadedDestWorker *
DestDriver::construct_worker(int worker_index)
{
  GrpcDestWorker *worker = grpc_dw_new(this->super, worker_index);
  worker->cpp = new DestWorker(worker);
  return &worker->super;
}


/* C Wrappers */

DestDriver *
pubsub_dd_get_cpp(GrpcDestDriver *self)
{
  return (DestDriver *) self->cpp;
}

LogDriver *
pubsub_dd_new(GlobalConfig *cfg)
{
  GrpcDestDriver *self = grpc_dd_new(cfg, "google_pubsub_grpc");
  self->cpp = new DestDriver(self);
  return &self->super.super.super;
}
