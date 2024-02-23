/*
 * Copyright (c) 2024 Attila Szakacs
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

#include "otel-logmsg-handles.hpp"

using namespace syslogng::grpc::otel::logmsg_handle;

NVHandle syslogng::grpc::otel::logmsg_handle::RAW_TYPE;
NVHandle syslogng::grpc::otel::logmsg_handle::TYPE;
NVHandle syslogng::grpc::otel::logmsg_handle::RAW_RESOURCE;
NVHandle syslogng::grpc::otel::logmsg_handle::RAW_RESOURCE_SCHEMA_URL;
NVHandle syslogng::grpc::otel::logmsg_handle::RESOURCE_DROPPED_ATTRIBUTES_COUNT;
NVHandle syslogng::grpc::otel::logmsg_handle::RESOURCE_SCHEMA_URL;
NVHandle syslogng::grpc::otel::logmsg_handle::RAW_SCOPE;
NVHandle syslogng::grpc::otel::logmsg_handle::RAW_SCOPE_SCHEMA_URL;
NVHandle syslogng::grpc::otel::logmsg_handle::SCOPE_DROPPED_ATTRIBUTES_COUNT;
NVHandle syslogng::grpc::otel::logmsg_handle::SCOPE_NAME;
NVHandle syslogng::grpc::otel::logmsg_handle::SCOPE_SCHEMA_URL;
NVHandle syslogng::grpc::otel::logmsg_handle::SCOPE_VERSION;
NVHandle syslogng::grpc::otel::logmsg_handle::RAW_LOG;
NVHandle syslogng::grpc::otel::logmsg_handle::LOG_BODY;
NVHandle syslogng::grpc::otel::logmsg_handle::LOG_DROPPED_ATTRIBUTES_COUNT;
NVHandle syslogng::grpc::otel::logmsg_handle::LOG_FLAGS;
NVHandle syslogng::grpc::otel::logmsg_handle::LOG_OBSERVED_TIME_UNIX_NANO;
NVHandle syslogng::grpc::otel::logmsg_handle::LOG_SEVERITY_NUMBER;
NVHandle syslogng::grpc::otel::logmsg_handle::LOG_SEVERITY_TEXT;
NVHandle syslogng::grpc::otel::logmsg_handle::LOG_SPAN_ID;
NVHandle syslogng::grpc::otel::logmsg_handle::LOG_TIME_UNIX_NANO;
NVHandle syslogng::grpc::otel::logmsg_handle::LOG_TRACE_ID;
NVHandle syslogng::grpc::otel::logmsg_handle::RAW_METRIC;
NVHandle syslogng::grpc::otel::logmsg_handle::METRIC_DATA_EXPONENTIAL_HISTOGRAM_AGGREGATION_TEMPORALITY;
NVHandle syslogng::grpc::otel::logmsg_handle::METRIC_DATA_HISTOGRAM_AGGREGATION_TEMPORALITY;
NVHandle syslogng::grpc::otel::logmsg_handle::METRIC_DATA_SUM_AGGREGATION_TEMPORALITY;
NVHandle syslogng::grpc::otel::logmsg_handle::METRIC_DATA_SUM_IS_MONOTONIC;
NVHandle syslogng::grpc::otel::logmsg_handle::METRIC_DATA_TYPE;
NVHandle syslogng::grpc::otel::logmsg_handle::METRIC_DESCRIPTION;
NVHandle syslogng::grpc::otel::logmsg_handle::METRIC_NAME;
NVHandle syslogng::grpc::otel::logmsg_handle::METRIC_UNIT;
NVHandle syslogng::grpc::otel::logmsg_handle::RAW_SPAN;
NVHandle syslogng::grpc::otel::logmsg_handle::SPAN_DROPPED_ATTRIBUTES_COUNT;
NVHandle syslogng::grpc::otel::logmsg_handle::SPAN_DROPPED_EVENTS_COUNT;
NVHandle syslogng::grpc::otel::logmsg_handle::SPAN_DROPPED_LINKS_COUNT;
NVHandle syslogng::grpc::otel::logmsg_handle::SPAN_END_TIME_UNIX_NANO;
NVHandle syslogng::grpc::otel::logmsg_handle::SPAN_KIND;
NVHandle syslogng::grpc::otel::logmsg_handle::SPAN_NAME;
NVHandle syslogng::grpc::otel::logmsg_handle::SPAN_PARENT_SPAN_ID;
NVHandle syslogng::grpc::otel::logmsg_handle::SPAN_SPAN_ID;
NVHandle syslogng::grpc::otel::logmsg_handle::SPAN_START_TIME_UNIX_NANO;
NVHandle syslogng::grpc::otel::logmsg_handle::SPAN_STATUS_CODE;
NVHandle syslogng::grpc::otel::logmsg_handle::SPAN_STATUS_MESSAGE;
NVHandle syslogng::grpc::otel::logmsg_handle::SPAN_TRACE_ID;
NVHandle syslogng::grpc::otel::logmsg_handle::SPAN_TRACE_STATE;

void
otel_logmsg_handles_global_init()
{
  RAW_TYPE = log_msg_get_value_handle(".otel_raw.type");
  TYPE = log_msg_get_value_handle(".otel.type");
  RAW_RESOURCE = log_msg_get_value_handle(".otel_raw.resource");
  RAW_RESOURCE_SCHEMA_URL = log_msg_get_value_handle(".otel_raw.resource_schema_url");
  RESOURCE_DROPPED_ATTRIBUTES_COUNT = log_msg_get_value_handle(".otel.resource.dropped_attributes_count");
  RESOURCE_SCHEMA_URL = log_msg_get_value_handle(".otel.resource.schema_url");
  RAW_SCOPE = log_msg_get_value_handle(".otel_raw.scope");
  RAW_SCOPE_SCHEMA_URL = log_msg_get_value_handle(".otel_raw.scope_schema_url");
  SCOPE_DROPPED_ATTRIBUTES_COUNT = log_msg_get_value_handle(".otel.scope.dropped_attributes_count");
  SCOPE_NAME = log_msg_get_value_handle(".otel.scope.name");
  SCOPE_SCHEMA_URL = log_msg_get_value_handle(".otel.scope.schema_url");
  SCOPE_VERSION = log_msg_get_value_handle(".otel.scope.version");
  RAW_LOG = log_msg_get_value_handle(".otel_raw.log");
  LOG_BODY = log_msg_get_value_handle(".otel.log.body");
  LOG_DROPPED_ATTRIBUTES_COUNT = log_msg_get_value_handle(".otel.log.dropped_attributes_count");
  LOG_FLAGS = log_msg_get_value_handle(".otel.log.flags");
  LOG_OBSERVED_TIME_UNIX_NANO = log_msg_get_value_handle(".otel.log.observed_time_unix_nano");
  LOG_SEVERITY_NUMBER = log_msg_get_value_handle(".otel.log.severity_number");
  LOG_SEVERITY_TEXT = log_msg_get_value_handle(".otel.log.severity_text");
  LOG_SPAN_ID = log_msg_get_value_handle(".otel.log.span_id");
  LOG_TIME_UNIX_NANO = log_msg_get_value_handle(".otel.log.time_unix_nano");
  LOG_TRACE_ID = log_msg_get_value_handle(".otel.log.trace_id");
  RAW_METRIC = log_msg_get_value_handle(".otel_raw.metric");
  METRIC_DATA_EXPONENTIAL_HISTOGRAM_AGGREGATION_TEMPORALITY =
    log_msg_get_value_handle(".otel.metric.data.exponential_histogram.aggregation_temporality");
  METRIC_DATA_HISTOGRAM_AGGREGATION_TEMPORALITY =
    log_msg_get_value_handle(".otel.metric.data.histogram.aggregation_temporality");
  METRIC_DATA_SUM_AGGREGATION_TEMPORALITY = log_msg_get_value_handle(".otel.metric.data.sum.aggregation_temporality");
  METRIC_DATA_SUM_IS_MONOTONIC = log_msg_get_value_handle(".otel.metric.data.sum.is_monotonic");
  METRIC_DATA_TYPE = log_msg_get_value_handle(".otel.metric.data.type");
  METRIC_DESCRIPTION = log_msg_get_value_handle(".otel.metric.description");
  METRIC_NAME = log_msg_get_value_handle(".otel.metric.name");
  METRIC_UNIT = log_msg_get_value_handle(".otel.metric.unit");
  RAW_SPAN = log_msg_get_value_handle(".otel_raw.span");
  SPAN_DROPPED_ATTRIBUTES_COUNT = log_msg_get_value_handle(".otel.span.dropped_attributes_count");
  SPAN_DROPPED_EVENTS_COUNT = log_msg_get_value_handle(".otel.span.dropped_events_count");
  SPAN_DROPPED_LINKS_COUNT = log_msg_get_value_handle(".otel.span.dropped_links_count");
  SPAN_END_TIME_UNIX_NANO = log_msg_get_value_handle(".otel.span.end_time_unix_nano");
  SPAN_KIND = log_msg_get_value_handle(".otel.span.kind");
  SPAN_NAME = log_msg_get_value_handle(".otel.span.name");
  SPAN_PARENT_SPAN_ID = log_msg_get_value_handle(".otel.span.parent_span_id");
  SPAN_SPAN_ID = log_msg_get_value_handle(".otel.span.span_id");
  SPAN_START_TIME_UNIX_NANO = log_msg_get_value_handle(".otel.span.start_time_unix_nano");
  SPAN_STATUS_CODE = log_msg_get_value_handle(".otel.span.status.code");
  SPAN_STATUS_MESSAGE = log_msg_get_value_handle(".otel.span.status.message");
  SPAN_TRACE_ID = log_msg_get_value_handle(".otel.span.trace_id");
  SPAN_TRACE_STATE = log_msg_get_value_handle(".otel.span.trace_state");
}
