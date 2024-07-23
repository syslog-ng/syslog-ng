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

#ifndef OTEL_DEST_WORKER_HPP
#define OTEL_DEST_WORKER_HPP

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>

#include "opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.h"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"
#include "opentelemetry/proto/logs/v1/logs.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/trace/v1/trace.pb.h"

#include "otel-dest.hpp"
#include "otel-protobuf-formatter.hpp"

typedef struct OtelDestWorker_ OtelDestWorker;

namespace syslogng {
namespace grpc {
namespace otel {

using opentelemetry::proto::collector::logs::v1::LogsService;
using opentelemetry::proto::collector::logs::v1::ExportLogsServiceRequest;
using opentelemetry::proto::collector::logs::v1::ExportLogsServiceResponse;
using opentelemetry::proto::collector::metrics::v1::MetricsService;
using opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceRequest;
using opentelemetry::proto::collector::metrics::v1::ExportMetricsServiceResponse;
using opentelemetry::proto::collector::trace::v1::TraceService;
using opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest;
using opentelemetry::proto::collector::trace::v1::ExportTraceServiceResponse;
using opentelemetry::proto::logs::v1::ScopeLogs;
using opentelemetry::proto::metrics::v1::ScopeMetrics;
using opentelemetry::proto::trace::v1::ScopeSpans;

class DestWorker
{
public:
  DestWorker(OtelDestWorker *s);
  virtual ~DestWorker() {};
  static LogThreadedDestWorker *construct(LogThreadedDestDriver *o, gint worker_index);

  virtual bool init();
  virtual void deinit();
  virtual bool connect();
  virtual void disconnect();
  virtual LogThreadedResult insert(LogMessage *msg);
  virtual LogThreadedResult flush(LogThreadedFlushMode mode);

protected:
  void prepare_context(::grpc::ClientContext &context);

  void clear_current_msg_metadata();
  void get_metadata_for_current_msg(LogMessage *msg);

  virtual ScopeLogs *lookup_scope_logs(LogMessage *msg);
  virtual ScopeLogs *lookup_fallback_scope_logs(LogMessage *msg);
  virtual ScopeMetrics *lookup_scope_metrics(LogMessage *msg);
  virtual ScopeSpans *lookup_scope_spans(LogMessage *msg);

  bool should_initiate_flush();

  bool insert_log_record_from_log_msg(LogMessage *msg);
  void insert_fallback_log_record_from_log_msg(LogMessage *msg);
  bool insert_metric_from_log_msg(LogMessage *msg);
  bool insert_span_from_log_msg(LogMessage *msg);

  LogThreadedResult flush_log_records();
  LogThreadedResult flush_metrics();
  LogThreadedResult flush_spans();

protected:
  OtelDestWorker *super;
  DestDriver &owner;

  std::shared_ptr<::grpc::Channel> channel;
  std::unique_ptr<LogsService::Stub> logs_service_stub;
  std::unique_ptr<MetricsService::Stub> metrics_service_stub;
  std::unique_ptr<TraceService::Stub> trace_service_stub;

  ExportLogsServiceRequest logs_service_request;
  ExportLogsServiceResponse logs_service_response;
  size_t logs_current_batch_bytes;
  ExportMetricsServiceRequest metrics_service_request;
  ExportMetricsServiceResponse metrics_service_response;
  size_t metrics_current_batch_bytes;
  ExportTraceServiceRequest trace_service_request;
  ExportTraceServiceResponse trace_service_response;
  size_t spans_current_batch_bytes;

  ProtobufFormatter formatter;

  struct
  {
    Resource resource;
    std::string resource_schema_url;
    InstrumentationScope scope;
    std::string scope_schema_url;
  } current_msg_metadata;

  ScopeLogs *fallback_msg_scope_logs = nullptr;
};

}
}
}

struct OtelDestWorker_
{
  LogThreadedDestWorker super;
  syslogng::grpc::otel::DestWorker *cpp;
};

void otel_dw_init_super(LogThreadedDestWorker *s, LogThreadedDestDriver *o, gint worker_index);

#endif
