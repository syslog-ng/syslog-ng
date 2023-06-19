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

#ifndef OTEL_PROTOBUF_PARSER_HPP
#define OTEL_PROTOBUF_PARSER_HPP

#include <grpcpp/support/config.h>

#include "compat/cpp-start.h"
#include "logmsg/logmsg.h"
#include "compat/cpp-end.h"

#include "opentelemetry/proto/resource/v1/resource.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"

namespace syslogng {
namespace grpc {
namespace otel {
namespace protobuf_parser {

using opentelemetry::proto::resource::v1::Resource;
using opentelemetry::proto::common::v1::InstrumentationScope;

void set_metadata(LogMessage *msg, const ::grpc::string &peer,
                  const Resource &resource, const std::string &resource_schema_url,
                  const InstrumentationScope &scope, const std::string &scope_schema_url);


}
}
}
}

#endif
