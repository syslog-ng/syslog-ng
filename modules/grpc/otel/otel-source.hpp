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

#ifndef OTEL_SOURCE_HPP
#define OTEL_SOURCE_HPP

#include <grpcpp/server.h>

#include "compat/cpp-start.h"
#include "logthrsource/logthrsourcedrv.h"
#include "compat/cpp-end.h"

#include "otel-source.h"
#include "otel-servicecall.hpp"
#include "credentials/grpc-credentials-builder.hpp"


namespace syslogng {
namespace grpc {
namespace otel {

class SourceDriver
{
public:
  SourceDriver(OtelSourceDriver *s);

  void run();
  void request_exit();
  void format_stats_key(StatsClusterKeyBuilder *kb);
  const char *generate_persist_name();
  gboolean init();
  gboolean deinit();

  GrpcServerCredentialsBuilderW *get_credentials_builder_wrapper();

  TraceService::AsyncService trace_service;
  LogsService::AsyncService logs_service;
  MetricsService::AsyncService metrics_service;

private:
  bool post(LogMessage *msg);

  friend TraceServiceCall;
  friend LogsServiceCall;
  friend MetricsServiceCall;

public:
  guint64 port = 4317;
  syslogng::grpc::ServerCredentialsBuilder credentials_builder;

private:
  OtelSourceDriver *super;
  GrpcServerCredentialsBuilderW credentials_builder_wrapper;
  std::unique_ptr<::grpc::Server> server;
  std::unique_ptr<::grpc::ServerCompletionQueue> cq;
};

}
}
}

struct OtelSourceDriver_
{
  LogThreadedSourceDriver super;
  syslogng::grpc::otel::SourceDriver *cpp;
};

#endif
