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

#ifndef OTEL_FIELD_HPP
#define OTEL_FIELD_HPP

#include "syslog-ng.h"
#include "protobuf-field.hpp"

namespace syslogng {
namespace grpc {
namespace otel {

using namespace google::protobuf;

class AnyField : public ProtobufField
{
  using AnyValue = opentelemetry::proto::common::v1::AnyValue;

public:
  FilterXObject *FilterXObjectGetter(Message *message, ProtoReflectors reflectors);
  bool FilterXObjectSetter(Message *message, ProtoReflectors reflectors, FilterXObject *object,
                           FilterXObject **assoc_object);

  FilterXObject *FilterXObjectDirectGetter(AnyValue *anyValue);
  bool FilterXObjectDirectSetter(AnyValue *anyValue, FilterXObject *object, FilterXObject **assoc_object);
};

extern AnyField any_field_converter;

ProtobufField *otel_converter_by_type(FieldDescriptor::Type fieldType);
ProtobufField *otel_converter_by_field_descriptor(const FieldDescriptor *fd);

}
}
}

#endif
