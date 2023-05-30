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

#include "otel-source-services.hpp"
#include "otel-protobuf-parser.hpp"

using namespace syslogng::grpc::otel;
using namespace opentelemetry::proto::collector::trace::v1;
using namespace opentelemetry::proto::collector::logs::v1;
using namespace opentelemetry::proto::collector::metrics::v1;
using namespace opentelemetry::proto::trace::v1;
using namespace opentelemetry::proto::logs::v1;
using namespace opentelemetry::proto::metrics::v1;
using namespace opentelemetry::proto::resource::v1;
using namespace opentelemetry::proto::common::v1;

::grpc::Status
syslogng::grpc::otel::SourceTraceService::Export(::grpc::ServerContext *context,
                                                 const ExportTraceServiceRequest *request,
                                                 ExportTraceServiceResponse *response)
{
  for (const ResourceSpans &resource_spans : request->resource_spans())
    {
      const Resource &resource = resource_spans.resource();
      const std::string &resource_logs_schema_url = resource_spans.schema_url();

      for (const ScopeSpans &scope_spans : resource_spans.scope_spans())
        {
          const InstrumentationScope &scope = scope_spans.scope();
          const std::string &scope_logs_schema_url = scope_spans.schema_url();

          for (const Span &span : scope_spans.spans())
            {
              LogMessage *msg = log_msg_new_empty();
              protobuf_parser::set_metadata(msg, context->peer(), resource, resource_logs_schema_url, scope,
                                            scope_logs_schema_url);
              log_msg_unref(msg);
            }
        }
    }

  return ::grpc::Status::OK;
}

::grpc::Status
syslogng::grpc::otel::SourceLogsService::Export(::grpc::ServerContext *context,
                                                const ExportLogsServiceRequest *request,
                                                ExportLogsServiceResponse *response)
{
  for (const ResourceLogs &resource_logs : request->resource_logs())
    {
      const Resource &resource = resource_logs.resource();
      const std::string &resource_logs_schema_url = resource_logs.schema_url();

      for (const ScopeLogs &scope_logs : resource_logs.scope_logs())
        {
          const InstrumentationScope &scope = scope_logs.scope();
          const std::string &scope_logs_schema_url = scope_logs.schema_url();

          for (const LogRecord &log_record : scope_logs.log_records())
            {
              LogMessage *msg = log_msg_new_empty();
              protobuf_parser::set_metadata(msg, context->peer(), resource, resource_logs_schema_url, scope,
                                            scope_logs_schema_url);
              protobuf_parser::parse(msg, log_record);
              log_msg_unref(msg);
            }
        }
    }

  return ::grpc::Status::OK;
}

::grpc::Status
syslogng::grpc::otel::SourceMetricsService::Export(::grpc::ServerContext *context,
                                                   const ExportMetricsServiceRequest *request,
                                                   ExportMetricsServiceResponse *response)
{
  for (const ResourceMetrics &resource_metrics : request->resource_metrics())
    {
      const Resource &resource = resource_metrics.resource();
      const std::string &resource_logs_schema_url = resource_metrics.schema_url();

      for (const ScopeMetrics &scope_metrics : resource_metrics.scope_metrics())
        {
          const InstrumentationScope &scope = scope_metrics.scope();
          const std::string &scope_logs_schema_url = scope_metrics.schema_url();

          for (const Metric &metric : scope_metrics.metrics())
            {
              LogMessage *msg = log_msg_new_empty();
              protobuf_parser::set_metadata(msg, context->peer(), resource, resource_logs_schema_url, scope,
                                            scope_logs_schema_url);
              log_msg_unref(msg);
            }
        }
    }

  return ::grpc::Status::OK;
}
