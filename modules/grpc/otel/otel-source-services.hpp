/*
 * Copyright (c) 2023 Attila Szakacs
 * Copyright (c) 2023 László Várady
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

#ifndef OTEL_SOURCE_SERVICES_HPP
#define OTEL_SOURCE_SERVICES_HPP

#include "opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"
#include "opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"

#include "otel-servicecall.hpp"
#include "otel-source.hpp"
#include "otel-protobuf-parser.hpp"

#include <grpcpp/grpcpp.h>

namespace syslogng {
namespace grpc {
namespace otel {

using opentelemetry::proto::resource::v1::Resource;
using opentelemetry::proto::common::v1::InstrumentationScope;
using opentelemetry::proto::trace::v1::ResourceSpans;
using opentelemetry::proto::trace::v1::ScopeSpans;
using opentelemetry::proto::trace::v1::Span;
using opentelemetry::proto::logs::v1::ResourceLogs;
using opentelemetry::proto::logs::v1::ScopeLogs;
using opentelemetry::proto::logs::v1::LogRecord;
using opentelemetry::proto::metrics::v1::ResourceMetrics;
using opentelemetry::proto::metrics::v1::ScopeMetrics;
using opentelemetry::proto::metrics::v1::Metric;

class AsyncServiceCallInterface
{
public:
  virtual void Proceed(bool ok) = 0;
  virtual ~AsyncServiceCallInterface() = default;
};

template <class S, class Req, class Res>
class AsyncServiceCall final : public AsyncServiceCallInterface
{
public:
  void Proceed(bool ok) override;

public:
  AsyncServiceCall(SourceWorker &worker_, S *service_, ::grpc::ServerCompletionQueue *cq_)
    : worker(worker_), service(service_), responder(&ctx), cq(cq_), status(PROCESS)
  {
    service->RequestExport(&ctx, &request, &responder, cq, cq, this);
  }

private:
  SourceWorker &worker;
  S *service;
  ::grpc::ServerAsyncResponseWriter<Res> responder;
  Req request;
  Res response;

  ::grpc::ServerCompletionQueue *cq;
  ::grpc::ServerContext ctx;

  enum CallStatus { PROCESS, FINISH };
  CallStatus status;
};

}
}
}

template <> void
syslogng::grpc::otel::TraceServiceCall::Proceed(bool ok)
{
  if (status == FINISH || !ok)
    {
      delete this;
      return;
    }

  if (!worker.super->super.under_termination)
    new TraceServiceCall(worker, service, cq);

  ::grpc::Status response_status = ::grpc::Status::OK;

  int msgs_in_fetch_round = 0;

  for (const ResourceSpans &resource_spans : request.resource_spans())
    {
      const Resource &resource = resource_spans.resource();
      const std::string &resource_spans_schema_url = resource_spans.schema_url();

      for (const ScopeSpans &scope_spans : resource_spans.scope_spans())
        {
          const InstrumentationScope &scope = scope_spans.scope();
          const std::string &scope_spans_schema_url = scope_spans.schema_url();

          for (const Span &span : scope_spans.spans())
            {
              if (worker.super->super.under_termination)
                {
                  response_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "Server is unavailable");
                  break;
                }

              LogMessage *msg = log_msg_new_empty();
              ProtobufParser::store_raw_metadata(msg, ctx.peer(), resource, resource_spans_schema_url, scope,
                                                 scope_spans_schema_url);
              ProtobufParser::store_raw(msg, span);
              worker.post(msg);

              msgs_in_fetch_round++;
              if (msgs_in_fetch_round == worker.driver.fetch_limit)
                {
                  log_threaded_source_worker_close_batch(&worker.super->super);
                  msgs_in_fetch_round = 0;
                }
            }
        }
    }

  if (msgs_in_fetch_round != 0)
    log_threaded_source_worker_close_batch(&worker.super->super);

  status = FINISH;
  responder.Finish(response, response_status, this);
}

template <> void
syslogng::grpc::otel::LogsServiceCall::Proceed(bool ok)
{
  if (status == FINISH || !ok)
    {
      delete this;
      return;
    }

  if (!worker.super->super.under_termination)
    new LogsServiceCall(worker, service, cq);

  ::grpc::Status response_status = ::grpc::Status::OK;

  int msgs_in_fetch_round = 0;

  for (const ResourceLogs &resource_logs : request.resource_logs())
    {
      const Resource &resource = resource_logs.resource();
      const std::string &resource_logs_schema_url = resource_logs.schema_url();

      for (const ScopeLogs &scope_logs : resource_logs.scope_logs())
        {
          const InstrumentationScope &scope = scope_logs.scope();
          const std::string &scope_logs_schema_url = scope_logs.schema_url();

          for (const LogRecord &log_record : scope_logs.log_records())
            {
              if (worker.super->super.under_termination)
                {
                  response_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "Server is unavailable");
                  break;
                }

              LogMessage *msg = log_msg_new_empty();
              if (ProtobufParser::is_syslog_ng_log_record(resource, resource_logs_schema_url, scope,
                                                          scope_logs_schema_url))
                {
                  ProtobufParser::store_syslog_ng(msg, log_record);
                }
              else
                {
                  ProtobufParser::store_raw_metadata(msg, ctx.peer(), resource, resource_logs_schema_url, scope,
                                                     scope_logs_schema_url);
                  ProtobufParser::store_raw(msg, log_record);
                }
              worker.post(msg);

              msgs_in_fetch_round++;
              if (msgs_in_fetch_round == worker.driver.fetch_limit)
                {
                  log_threaded_source_worker_close_batch(&worker.super->super);
                  msgs_in_fetch_round = 0;
                }
            }
        }
    }

  if (msgs_in_fetch_round != 0)
    log_threaded_source_worker_close_batch(&worker.super->super);

  status = FINISH;
  responder.Finish(response, response_status, this);
}

template <> void
syslogng::grpc::otel::MetricsServiceCall::Proceed(bool ok)
{
  if (status == FINISH || !ok)
    {
      delete this;
      return;
    }

  if (!worker.super->super.under_termination)
    new MetricsServiceCall(worker, service, cq);

  ::grpc::Status response_status = ::grpc::Status::OK;

  int msgs_in_fetch_round = 0;

  for (const ResourceMetrics &resource_metrics : request.resource_metrics())
    {
      const Resource &resource = resource_metrics.resource();
      const std::string &resource_metrics_schema_url = resource_metrics.schema_url();

      for (const ScopeMetrics &scope_metrics : resource_metrics.scope_metrics())
        {
          const InstrumentationScope &scope = scope_metrics.scope();
          const std::string &scope_metrics_schema_url = scope_metrics.schema_url();

          for (const Metric &metric : scope_metrics.metrics())
            {
              if (worker.super->super.under_termination)
                {
                  response_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "Server is unavailable");
                  break;
                }

              LogMessage *msg = log_msg_new_empty();
              ProtobufParser::store_raw_metadata(msg, ctx.peer(), resource, resource_metrics_schema_url, scope,
                                                 scope_metrics_schema_url);
              ProtobufParser::store_raw(msg, metric);
              worker.post(msg);

              msgs_in_fetch_round++;
              if (msgs_in_fetch_round == worker.driver.fetch_limit)
                {
                  log_threaded_source_worker_close_batch(&worker.super->super);
                  msgs_in_fetch_round = 0;
                }
            }
        }
    }

  if (msgs_in_fetch_round != 0)
    log_threaded_source_worker_close_batch(&worker.super->super);

  status = FINISH;
  responder.Finish(response, response_status, this);
}

#endif
