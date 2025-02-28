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

#include "otel-dest.hpp"
#include "otel-dest-worker.hpp"

using namespace syslogng::grpc::otel;

/* C++ Implementations */

DestDriver::DestDriver(GrpcDestDriver *s) :
  syslogng::grpc::DestDriver(s)
{
  this->enable_dynamic_headers();
}

const char *
DestDriver::generate_persist_name()
{
  static char persist_name[1024];

  if (super->super.super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "opentelemetry.%s",
               super->super.super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "opentelemetry(%s)",
               url.c_str());

  return persist_name;
}

const char *
DestDriver::format_stats_key(StatsClusterKeyBuilder *kb)
{
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "opentelemetry"));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("url", url.c_str()));

  return NULL;
}

LogThreadedDestWorker *
DestDriver::construct_worker(int worker_index)
{
  GrpcDestWorker *worker = grpc_dw_new(this->super, worker_index);
  try
    {
      worker->cpp = new DestWorker(worker);
    }
  catch (const std::runtime_error &e)
    {
      log_threaded_dest_worker_free(&worker->super);
      return NULL;
    }
  return &worker->super;
}

LogDriver *
otel_dd_new(GlobalConfig *cfg)
{
  GrpcDestDriver *self = grpc_dd_new(cfg, "opentelemetry");
  self->cpp = new DestDriver(self);
  return &self->super.super.super;
}
