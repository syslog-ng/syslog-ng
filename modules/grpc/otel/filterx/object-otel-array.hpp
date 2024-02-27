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

#ifndef OBJECT_OTEL_ARRAY_HPP
#define OBJECT_OTEL_ARRAY_HPP

#include "syslog-ng.h"

#include "compat/cpp-start.h"
#include "filterx/filterx-object.h"
#include "object-otel.h"
#include "compat/cpp-end.h"

#include "protobuf-field.hpp"
#include "opentelemetry/proto/common/v1/common.pb.h"

typedef struct FilterXOtelArray_ FilterXOtelArray;

FilterXObject *_filterx_otel_array_clone(FilterXObject *s);

namespace syslogng {
namespace grpc {
namespace otel {
namespace filterx {

class Array
{
public:
  Array(FilterXOtelArray *s);
  Array(FilterXOtelArray *s, FilterXObject *protobuf_object);
  Array(Array &o) = delete;
  Array(Array &&o) = delete;

  std::string marshal();
  bool set_subscript(FilterXObject *key, FilterXObject *value);
  FilterXObject *get_subscript(FilterXObject *key);
  const opentelemetry::proto::common::v1::ArrayValue &get_value() const;

private:
  Array(const Array &o, FilterXOtelArray *s);
  friend FilterXObject *::_filterx_otel_array_clone(FilterXObject *s);

private:
  FilterXOtelArray *super;
  opentelemetry::proto::common::v1::ArrayValue array;

  friend class OtelArrayField;
};

class OtelArrayField : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(const google::protobuf::Message &message, ProtoReflectors reflectors);
  bool FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object);
};

extern OtelArrayField otel_array_converter;

}
}
}
}

struct FilterXOtelArray_
{
  FilterXObject super;
  syslogng::grpc::otel::filterx::Array *cpp;
};

#endif
