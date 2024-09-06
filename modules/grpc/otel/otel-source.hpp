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

#ifndef OTEL_SOURCE_HPP
#define OTEL_SOURCE_HPP

#include "otel-source.h"

#include "grpc-source.hpp"
#include "grpc-source-worker.hpp"
#include "otel-servicecall.hpp"


namespace syslogng {
namespace grpc {
namespace otel {

class SourceDriver : public syslogng::grpc::SourceDriver
{
public:
  SourceDriver(GrpcSourceDriver *s);

  gboolean init() override;
  gboolean deinit() override;
  void request_exit();
  void format_stats_key(StatsClusterKeyBuilder *kb);
  const char *generate_persist_name();
  LogThreadedSourceWorker *construct_worker(int worker_index);

  std::unique_ptr<TraceService::AsyncService> trace_service;
  std::unique_ptr<LogsService::AsyncService> logs_service;
  std::unique_ptr<MetricsService::AsyncService> metrics_service;

private:
  friend class SourceWorker;

private:
  std::unique_ptr<::grpc::Server> server;
  std::list<std::unique_ptr<::grpc::ServerCompletionQueue>> cqs;
};

class SourceWorker : public syslogng::grpc::SourceWorker
{
public:
  SourceWorker(GrpcSourceWorker *s, syslogng::grpc::SourceDriver &d);

  void run();
  void request_exit();

private:
  friend TraceServiceCall;
  friend LogsServiceCall;
  friend MetricsServiceCall;

private:
  std::unique_ptr<::grpc::ServerCompletionQueue> cq;
};

}
}
}

syslogng::grpc::otel::SourceDriver *otel_sd_get_cpp(GrpcSourceDriver *self);

#endif
