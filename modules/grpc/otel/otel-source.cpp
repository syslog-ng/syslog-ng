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

#include "otel-source.hpp"
#include "otel-source-services.hpp"

#include "compat/cpp-start.h"
#include "messages.h"
#include "compat/cpp-end.h"

#include <string>

#include <grpcpp/grpcpp.h>

using namespace syslogng::grpc::otel;

SourceDriver::SourceDriver(GrpcSourceDriver *s)
  : syslogng::grpc::SourceDriver(s)
{
  this->port = 4317;
}

void
SourceDriver::request_exit()
{
  msg_debug("Shutting down OpenTelemetry server", evt_tag_int("port", this->port));
  server->Shutdown(std::chrono::system_clock::now() + std::chrono::seconds(30));
}

void
SourceDriver::format_stats_key(StatsClusterKeyBuilder *kb)
{
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "opentelemetry"));

  gchar num[64];
  g_snprintf(num, sizeof(num), "%" G_GUINT32_FORMAT, this->port);
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("port", num));
}

const char *
SourceDriver::generate_persist_name()
{
  static char persist_name[1024];

  if (this->super->super.super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "opentelemetry.%s",
               this->super->super.super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "opentelemetry(%" G_GUINT32_FORMAT ")",
               this->port);

  return persist_name;
}

gboolean
SourceDriver::init()
{
  this->super->super.worker_options.super.keep_hostname = TRUE;

  ::grpc::ServerBuilder builder;
  if (!this->prepare_server_builder(builder))
    return FALSE;

  trace_service = std::make_unique<TraceService::AsyncService>();
  logs_service = std::make_unique<LogsService::AsyncService>();
  metrics_service = std::make_unique<MetricsService::AsyncService>();

  builder.RegisterService(this->trace_service.get());
  builder.RegisterService(this->logs_service.get());
  builder.RegisterService(this->metrics_service.get());

  for (int i = 0; i < this->super->super.num_workers; i++)
    this->cqs.push_back(builder.AddCompletionQueue());

  server = builder.BuildAndStart();
  if (!server)
    {
      msg_error("Failed to start OpenTelemetry server", evt_tag_int("port", this->port));
      return FALSE;
    }

  if (!syslogng::grpc::SourceDriver::init())
    return FALSE;

  msg_info("OpenTelemetry server accepting connections", evt_tag_int("port", this->port));
  return TRUE;
}

gboolean
SourceDriver::deinit()
{
  this->trace_service = nullptr;
  this->logs_service = nullptr;
  this->metrics_service = nullptr;

  return syslogng::grpc::SourceDriver::deinit();
}

LogThreadedSourceWorker *
SourceDriver::construct_worker(int worker_index)
{
  GrpcSourceWorker *worker = grpc_sw_new(this->super, worker_index);
  worker->cpp = new SourceWorker(worker, *this);
  return &worker->super;
}


SourceWorker::SourceWorker(GrpcSourceWorker *s, syslogng::grpc::SourceDriver &d)
  : syslogng::grpc::SourceWorker(s, d)
{
  SourceDriver *owner_ = otel_sd_get_cpp(this->driver.super);
  cq = std::move(owner_->cqs.front());
  owner_->cqs.pop_front();
}

void
syslogng::grpc::otel::SourceWorker::run()
{
  SourceDriver *owner_ = otel_sd_get_cpp(this->driver.super);

  /* Proceed() will immediately create a new ServiceCall,
   * so creating 1 ServiceCall here results in 2 concurrent requests.
   *
   * Because of this we should create (concurrent_requests - 1) ServiceCalls here.
   */
  for (int i = 0; i < owner_->concurrent_requests - 1; i++)
    {
      new TraceServiceCall(*this, owner_->trace_service.get(), this->cq.get());
      new LogsServiceCall(*this, owner_->logs_service.get(), this->cq.get());
      new MetricsServiceCall(*this, owner_->metrics_service.get(), this->cq.get());
    }

  void *tag;
  bool ok;
  while (this->cq->Next(&tag, &ok))
    {
      static_cast<AsyncServiceCallInterface *>(tag)->Proceed(ok);
    }
}

void
syslogng::grpc::otel::SourceWorker::request_exit()
{
  this->driver.request_exit();
  this->cq->Shutdown();
}

SourceDriver *
otel_sd_get_cpp(GrpcSourceDriver *self)
{
  return (SourceDriver *) self->cpp;
}

LogDriver *
otel_sd_new(GlobalConfig *cfg)
{
  GrpcSourceDriver *self = grpc_sd_new(cfg, "opentelemetry", "otlp");
  self->cpp = new SourceDriver(self);
  return &self->super.super.super;
}
