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

#include <string>

#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>

#include "otel-source.hpp"
#include "otel-source-services.hpp"
#include "grpc-credentials-builder.hpp"

#include "compat/cpp-start.h"
#include "messages.h"
#include "compat/cpp-end.h"

#define get_SourceDriver(s) (((OtelSourceDriver *) s)->cpp)

using namespace syslogng::grpc::otel;

/* C++ Implementations */

syslogng::grpc::otel::SourceDriver::SourceDriver(OtelSourceDriver *s)
  : super(s)
{
  credentials_builder_wrapper.self = &credentials_builder;
}

void
syslogng::grpc::otel::SourceDriver::run()
{
  std::string address = std::string("[::]:").append(std::to_string(port));

  ::grpc::EnableDefaultHealthCheckService(true);

  ::grpc::ServerBuilder builder;
  builder.AddListeningPort(address, credentials_builder.build());

  TraceService::AsyncService trace_service;
  LogsService::AsyncService logs_service;
  MetricsService::AsyncService metrics_service;

  builder.RegisterService(&trace_service);
  builder.RegisterService(&logs_service);
  builder.RegisterService(&metrics_service);

  cq = builder.AddCompletionQueue();
  server = builder.BuildAndStart();
  if (!server)
    {
      msg_error("Failed to start OpenTelemetry server", evt_tag_int("port", port));
      return;
    }

  msg_info("OpenTelemetry server accepting connections", evt_tag_int("port", port));

  new TraceServiceCall(*this, &trace_service, cq.get());
  new LogsServiceCall(*this, &logs_service, cq.get());
  new MetricsServiceCall(*this, &metrics_service, cq.get());

  void *tag;
  bool ok;
  while (cq->Next(&tag, &ok))
    {
      static_cast<AsyncServiceCallInterface *>(tag)->Proceed(ok);
    }
}

void
syslogng::grpc::otel::SourceDriver::request_exit()
{
  server->Shutdown();
  cq->Shutdown();
}

void
syslogng::grpc::otel::SourceDriver::format_stats_key(StatsClusterKeyBuilder *kb)
{
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "opentelemetry"));

  gchar num[64];
  g_snprintf(num, sizeof(num), "%" G_GUINT64_FORMAT, port);
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("port", num));
}

const char *
syslogng::grpc::otel::SourceDriver::generate_persist_name()
{
  static char persist_name[1024];

  if (super->super.super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "opentelemetry.%s",
               super->super.super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "opentelemetry(%" G_GUINT64_FORMAT ")",
               port);

  return persist_name;
}

gboolean
syslogng::grpc::otel::SourceDriver::init()
{
  if (!credentials_builder.validate())
    return FALSE;

  return log_threaded_source_driver_init_method(&super->super.super.super.super);
}

gboolean
syslogng::grpc::otel::SourceDriver::deinit()
{
  return log_threaded_source_driver_deinit_method(&super->super.super.super.super);
}

bool
syslogng::grpc::otel::SourceDriver::post(LogMessage *msg)
{
  if (!log_threaded_source_free_to_send(&super->super))
    return false;

  log_threaded_source_post(&super->super, msg);
  return true;
}

GrpcServerCredentialsBuilderW *
SourceDriver::get_credentials_builder_wrapper()
{
  return &credentials_builder_wrapper;
}

/* Config setters */

void
otel_sd_set_port(LogDriver *s, guint64 port)
{
  get_SourceDriver(s)->port = port;
}

GrpcServerCredentialsBuilderW *
otel_sd_get_credentials_builder(LogDriver *s)
{
  return get_SourceDriver(s)->get_credentials_builder_wrapper();
}

/* C Wrappers */

static void
_run(LogThreadedSourceDriver *s)
{
  get_SourceDriver(s)->run();
}

static void
_request_exit(LogThreadedSourceDriver *s)
{
  get_SourceDriver(s)->request_exit();
}

static void
_format_stats_key(LogThreadedSourceDriver *s, StatsClusterKeyBuilder *kb)
{
  get_SourceDriver(s)->format_stats_key(kb);
}

static const gchar *
_generate_persist_name(const LogPipe *s)
{
  return get_SourceDriver(s)->generate_persist_name();
}

static gboolean
_init(LogPipe *s)
{
  return get_SourceDriver(s)->init();
}

static gboolean
_deinit(LogPipe *s)
{
  return get_SourceDriver(s)->deinit();
}

static void
_free(LogPipe *s)
{
  delete get_SourceDriver(s);
  log_threaded_source_driver_free_method(s);
}

LogDriver *
otel_sd_new(GlobalConfig *cfg)
{
  OtelSourceDriver *s = g_new0(OtelSourceDriver, 1);
  log_threaded_source_driver_init_instance(&s->super, cfg);

  s->cpp = new syslogng::grpc::otel::SourceDriver(s);

  s->super.super.super.super.init = _init;
  s->super.super.super.super.deinit = _deinit;
  s->super.super.super.super.free_fn = _free;
  s->super.super.super.super.generate_persist_name = _generate_persist_name;

  s->super.worker_options.super.stats_source = stats_register_type("opentelemetry");
  s->super.format_stats_key = _format_stats_key;
  s->super.run = _run;
  s->super.request_exit = _request_exit;

  return &s->super.super.super;
}
