/*
 * Copyright (c) 2023 shifter
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

#ifndef OTEL_LOG_RECORD_HPP
#define OTEL_LOG_RECORD_HPP

#include "syslog-ng.h"

#include "compat/cpp-start.h"
#include "filterx/filterx-object.h"
#include "otel-logrecord.h"
#include "compat/cpp-end.h"

#include "opentelemetry/proto/logs/v1/logs.pb.h"

typedef struct FilterXOtelLogRecord_ FilterXOtelLogRecord;

FilterXObject *_filterx_otel_logrecord_clone(FilterXObject *s);

namespace syslogng {
namespace grpc {
namespace otel {

using opentelemetry::proto::logs::v1::LogRecord;

class OtelLogRecordCpp
{
public:
  OtelLogRecordCpp(FilterXOtelLogRecord *folr);
  OtelLogRecordCpp(FilterXOtelLogRecord *folr, FilterXObject *protobuf_object);
  OtelLogRecordCpp(OtelLogRecordCpp &o) = delete;
  OtelLogRecordCpp(OtelLogRecordCpp &&o) = delete;
  FilterXObject *FilterX();
  bool SetField(const gchar *attribute, FilterXObject *value);
  std::string Marshal(void);
  FilterXObject *GetField(const gchar *attribute);
  const LogRecord &GetValue() const;
private:
  FilterXOtelLogRecord *super;
  LogRecord logRecord;
  OtelLogRecordCpp(const OtelLogRecordCpp &o, FilterXOtelLogRecord *folr);
  friend FilterXObject *::_filterx_otel_logrecord_clone(FilterXObject *s);
};

}
}
}

struct FilterXOtelLogRecord_
{
  FilterXObject super;
  syslogng::grpc::otel::OtelLogRecordCpp *cpp;
};

#endif
