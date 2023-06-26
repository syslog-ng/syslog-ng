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

#include "compat/cpp-start.h"
#include "logmsg/logmsg.h"
#include "compat/cpp-end.h"

#include "opentelemetry/proto/logs/v1/logs.pb.h"

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
using opentelemetry::proto::logs::v1::LogRecord;

class ProtobufFormatter
{
public:
  ProtobufFormatter(GlobalConfig *cfg);

  bool get_metadata(LogMessage *msg, Resource &resource, std::string &resource_schema_url,
                    InstrumentationScope &scope, std::string &scope_schema_url);
  bool format(LogMessage *msg, LogRecord &log_record);
  void format_fallback(LogMessage *msg, LogRecord &log_record);

private:
  void get_and_set_repeated_KeyValues(LogMessage *msg, const char *prefix, RepeatedPtrField<KeyValue> *key_values);
  bool get_resource_and_schema_url(LogMessage *msg, Resource &resource, std::string &schema_url);
  bool get_scope_and_schema_url(LogMessage *msg, InstrumentationScope &scope, std::string &schema_url);

private:
  GlobalConfig *cfg;
};

}
}
}

#endif
