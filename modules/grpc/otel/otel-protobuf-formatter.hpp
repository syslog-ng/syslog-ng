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

#ifndef OTEL_PROTOBUF_FORMATTER_HPP
#define OTEL_PROTOBUF_FORMATTER_HPP

#include "syslog-ng.h"

#include "compat/cpp-start.h"
#include "logmsg/logmsg.h"
#include "value-pairs/value-pairs.h"
#include "compat/cpp-end.h"

#include "opentelemetry/proto/logs/v1/logs.pb.h"
#include "opentelemetry/proto/metrics/v1/metrics.pb.h"
#include "opentelemetry/proto/trace/v1/trace.pb.h"

namespace syslogng {
namespace grpc {
namespace otel {

enum MessageType : int
{
  UNKNOWN,
  LOG,
  METRIC,
  SPAN,
};

MessageType get_message_type(LogMessage *msg);

using google::protobuf::RepeatedPtrField;

using opentelemetry::proto::resource::v1::Resource;
using opentelemetry::proto::common::v1::InstrumentationScope;
using opentelemetry::proto::common::v1::KeyValue;
using opentelemetry::proto::common::v1::KeyValueList;
using opentelemetry::proto::logs::v1::LogRecord;
using opentelemetry::proto::metrics::v1::Metric;
using opentelemetry::proto::metrics::v1::Gauge;
using opentelemetry::proto::metrics::v1::Sum;
using opentelemetry::proto::metrics::v1::Histogram;
using opentelemetry::proto::metrics::v1::ExponentialHistogram;
using opentelemetry::proto::metrics::v1::Summary;
using opentelemetry::proto::metrics::v1::Exemplar;
using opentelemetry::proto::metrics::v1::NumberDataPoint;
using opentelemetry::proto::metrics::v1::SummaryDataPoint;
using opentelemetry::proto::metrics::v1::HistogramDataPoint;
using opentelemetry::proto::metrics::v1::ExponentialHistogramDataPoint;
using opentelemetry::proto::trace::v1::Span;

class ProtobufFormatter
{
public:
  ProtobufFormatter(GlobalConfig *cfg);

  static void get_metadata_for_syslog_ng(Resource &resource, std::string &resource_schema_url,
                                         InstrumentationScope &scope, std::string &scope_schema_url);
  bool get_metadata(LogMessage *msg, Resource &resource, std::string &resource_schema_url,
                    InstrumentationScope &scope, std::string &scope_schema_url);
  bool format(LogMessage *msg, LogRecord &log_record);
  void format_fallback(LogMessage *msg, LogRecord &log_record);
  void format_syslog_ng(LogMessage *msg, LogRecord &log_record);
  bool format(LogMessage *msg, Metric &metric);
  bool format(LogMessage *msg, Span &span);

private:
  void get_and_set_repeated_KeyValues(LogMessage *msg, const char *prefix, RepeatedPtrField<KeyValue> *key_values);
  bool get_resource_and_schema_url(LogMessage *msg, Resource &resource, std::string &schema_url);
  bool get_scope_and_schema_url(LogMessage *msg, InstrumentationScope &scope, std::string &schema_url);

  /* Metric */
  void add_exemplars(LogMessage *msg, std::string &key_buffer, RepeatedPtrField<Exemplar> *exemplars);
  void add_number_data_points(LogMessage *msg, const char *prefix, RepeatedPtrField<NumberDataPoint> *data_points);
  void set_metric_gauge_values(LogMessage *msg, Gauge *gauge);
  void set_metric_sum_values(LogMessage *msg, Sum *sum);
  void add_histogram_data_points(LogMessage *msg, const char *prefix,
                                 RepeatedPtrField<HistogramDataPoint> *data_points);
  void set_metric_histogram_values(LogMessage *msg, Histogram *histogram);
  void add_exponential_histogram_data_points(LogMessage *msg, const char *prefix,
                                             RepeatedPtrField<ExponentialHistogramDataPoint> *data_points);
  void set_metric_exponential_histogram_values(LogMessage *msg, ExponentialHistogram *exponential_histogram);
  void add_summary_data_points(LogMessage *msg, const char *prefix, RepeatedPtrField<SummaryDataPoint> *data_points);
  void set_metric_summary_values(LogMessage *msg, Summary *summary);

  /* syslog-ng */
  void set_syslog_ng_nv_pairs(LogMessage *msg, LogRecord &log_record);
  void set_syslog_ng_macros(LogMessage *msg, LogRecord &log_record);
  void set_syslog_ng_addresses(LogMessage *msg, LogRecord &log_record);
  void set_syslog_ng_address_attrs(GSockAddr *sa, KeyValueList *address_attrs, bool include_port);

private:
  GlobalConfig *cfg;
};

}
}
}

#endif
