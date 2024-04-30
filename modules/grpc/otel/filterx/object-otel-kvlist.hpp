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

#ifndef OBJECT_OTEL_KVLIST_HPP
#define OBJECT_OTEL_KVLIST_HPP

#include "syslog-ng.h"

#include "compat/cpp-start.h"
#include "filterx/object-dict-interface.h"
#include "object-otel.h"
#include "compat/cpp-end.h"

#include "protobuf-field.hpp"
#include "opentelemetry/proto/common/v1/common.pb.h"

typedef struct FilterXOtelKVList_ FilterXOtelKVList;

FilterXObject *_filterx_otel_kvlist_clone(FilterXObject *s);

namespace syslogng {
namespace grpc {
namespace otel {
namespace filterx {

using opentelemetry::proto::common::v1::KeyValue;
using google::protobuf::RepeatedPtrField;

class KVList
{

public:
  KVList(FilterXOtelKVList *s);
  KVList(FilterXOtelKVList *s, RepeatedPtrField<KeyValue > *k);
  KVList(FilterXOtelKVList *s, FilterXObject *protobuf_object);
  KVList(KVList &o) = delete;
  KVList(KVList &&o) = delete;
  ~KVList();

  std::string marshal();
  bool set_subscript(FilterXObject *key, FilterXObject **value);
  bool is_key_set(FilterXObject *key) const;
  FilterXObject *get_subscript(FilterXObject *key);
  bool unset_key(FilterXObject *key);
  uint64_t len() const;
  bool iter(FilterXDictIterFunc func, gpointer user_data) const;
  const RepeatedPtrField<KeyValue> &get_value() const;

private:
  KVList(const KVList &o, FilterXOtelKVList *s);
  friend FilterXObject *::_filterx_otel_kvlist_clone(FilterXObject *s);
  KeyValue *get_mutable_kv_for_key(const char *key) const;

private:
  FilterXOtelKVList *super;
  RepeatedPtrField<KeyValue> *repeated_kv;
  bool borrowed;

  friend class OtelKVListField;
};

class OtelKVListField : public ProtobufField
{
public:
  FilterXObject *FilterXObjectGetter(google::protobuf::Message *message, ProtoReflectors reflectors);
  bool FilterXObjectSetter(google::protobuf::Message *message, ProtoReflectors reflectors, FilterXObject *object,
                           FilterXObject **assoc_object);
};

extern OtelKVListField otel_kvlist_converter;

}
}
}
}

struct FilterXOtelKVList_
{
  FilterXDict super;
  syslogng::grpc::otel::filterx::KVList *cpp;
};

#endif
