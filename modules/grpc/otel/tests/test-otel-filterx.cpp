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

#include "filterx/object-otel-logrecord.hpp"
#include "filterx/object-otel-resource.hpp"
#include "filterx/object-otel-scope.hpp"
#include "filterx/object-otel-kvlist.hpp"

#include "compat/cpp-start.h"
#include "filterx/object-string.h"
#include "filterx/object-primitive.h"
#include "apphook.h"
#include "cfg.h"
#include "compat/cpp-end.h"

#include <google/protobuf/util/message_differencer.h>

#include <criterion/criterion.h>

using namespace syslogng::grpc::otel;
using namespace opentelemetry::proto::logs::v1;
using namespace opentelemetry::proto::common::v1;
using namespace opentelemetry::proto::resource::v1;
using namespace google::protobuf::util;

static void
_assert_filterx_integer_attribute(FilterXObject *obj, const std::string &attribute_name, gint64 expected_value)
{
  FilterXObject *filterx_integer = filterx_object_getattr(obj, attribute_name.c_str());
  cr_assert(filterx_object_is_type(filterx_integer, &FILTERX_TYPE_NAME(integer)));

  GenericNumber value = filterx_primitive_get_value(filterx_integer);
  cr_assert_eq(gn_as_int64(&value), expected_value);

  filterx_object_unref(filterx_integer);
}

static void
_assert_filterx_string_attribute(FilterXObject *obj, const std::string &attribute_name,
                                 const std::string &expected_value)
{
  FilterXObject *filterx_string = filterx_object_getattr(obj, attribute_name.c_str());
  cr_assert(filterx_object_is_type(filterx_string, &FILTERX_TYPE_NAME(string)));

  gsize len;
  const gchar *value = filterx_string_get_value(filterx_string, &len);
  cr_assert_eq(expected_value.compare(std::string(value, len)), 0);

  filterx_object_unref(filterx_string);
}

static void
_assert_filterx_repeated_kv_attribute(FilterXObject *obj, const std::string &attribute_name,
                                      const google::protobuf::RepeatedPtrField<KeyValue> &expected_repeated_kv)
{
  FilterXObject *filterx_otel_kvlist = filterx_object_getattr(obj, attribute_name.c_str());
  cr_assert(filterx_object_is_type(filterx_otel_kvlist, &FILTERX_TYPE_NAME(otel_kvlist)));

  const KeyValueList &kvlist = ((FilterXOtelKVList *) filterx_otel_kvlist)->cpp->get_value();
  cr_assert_eq(kvlist.values_size(), expected_repeated_kv.size());
  for (int i = 0; i < kvlist.values_size(); i++)
    {
      cr_assert(MessageDifferencer::Equals(kvlist.values(i), expected_repeated_kv.at(i)));
    }

  filterx_object_unref(filterx_otel_kvlist);
}

static void
_assert_filterx_integer_element(FilterXObject *obj, FilterXObject *key, gint64 expected_value)
{
  FilterXObject *filterx_integer = filterx_object_get_subscript(obj, key);
  cr_assert(filterx_object_is_type(filterx_integer, &FILTERX_TYPE_NAME(integer)));

  GenericNumber value = filterx_primitive_get_value(filterx_integer);
  cr_assert_eq(gn_as_int64(&value), expected_value);

  filterx_object_unref(filterx_integer);
}

static void
_assert_filterx_string_element(FilterXObject *obj, FilterXObject *key,
                               const std::string &expected_value)
{
  FilterXObject *filterx_string = filterx_object_get_subscript(obj, key);
  cr_assert(filterx_object_is_type(filterx_string, &FILTERX_TYPE_NAME(string)));

  gsize len;
  const gchar *value = filterx_string_get_value(filterx_string, &len);
  cr_assert_eq(expected_value.compare(std::string(value, len)), 0);

  filterx_object_unref(filterx_string);
}

static void
_assert_filterx_otel_kvlist_element(FilterXObject *obj, FilterXObject *key,
                                    const KeyValueList &expected_otel_kvlist)
{
  FilterXObject *filterx_otel_kvlist = filterx_object_get_subscript(obj, key);
  cr_assert(filterx_otel_kvlist);
  cr_assert(filterx_object_is_type(filterx_otel_kvlist, &FILTERX_TYPE_NAME(otel_kvlist)));

  const KeyValueList &kvlist = ((FilterXOtelKVList *) filterx_otel_kvlist)->cpp->get_value();
  cr_assert(MessageDifferencer::Equals(kvlist, expected_otel_kvlist));

  filterx_object_unref(filterx_otel_kvlist);
}


/* LogRecord */

Test(otel_filterx, logrecord_empty)
{
  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) otel_logrecord_new(NULL);
  cr_assert(filterx_otel_logrecord);

  cr_assert(MessageDifferencer::Equals(LogRecord(), filterx_otel_logrecord->cpp->GetValue()));

  filterx_object_unref(&filterx_otel_logrecord->super);
}

Test(otel_filterx, logrecord_from_protobuf)
{
  LogRecord log_record;
  log_record.mutable_body()->set_string_value("foobar");
  log_record.set_observed_time_unix_nano(1234);
  KeyValue *attribute = log_record.add_attributes();
  attribute->set_key("attribute_key");
  attribute->mutable_value()->set_int_value(42);

  std::string serialized_log_record = log_record.SerializePartialAsString();
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new(serialized_log_record.c_str(), serialized_log_record.length()));

  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) otel_logrecord_new(args);
  cr_assert(filterx_otel_logrecord);

  const LogRecord &log_record_from_filterx = filterx_otel_logrecord->cpp->GetValue();
  cr_assert(MessageDifferencer::Equals(log_record, log_record_from_filterx));

  filterx_object_unref(&filterx_otel_logrecord->super);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, logrecord_from_protobuf_invalid_arg)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("", 0));

  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) otel_logrecord_new(args);
  cr_assert_not(filterx_otel_logrecord);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, logrecord_from_protobuf_malformed_data)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new("1234", 4));

  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) otel_logrecord_new(args);
  cr_assert_not(filterx_otel_logrecord);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, logrecord_too_many_args)
{
  GPtrArray *args = g_ptr_array_new_full(2, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("foo", 3));
  g_ptr_array_insert(args, 1, filterx_protobuf_new("bar", 3));

  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) otel_logrecord_new(args);
  cr_assert_not(filterx_otel_logrecord);

  g_ptr_array_free(args, TRUE);
}


/* Resource */

Test(otel_filterx, resource_empty)
{
  FilterXOtelResource *filterx_otel_resource = (FilterXOtelResource *) otel_resource_new(NULL);
  cr_assert(filterx_otel_resource);

  cr_assert(MessageDifferencer::Equals(opentelemetry::proto::resource::v1::Resource(),
                                       filterx_otel_resource->cpp->get_value()));

  filterx_object_unref(&filterx_otel_resource->super);
}

Test(otel_filterx, resource_from_protobuf)
{
  opentelemetry::proto::resource::v1::Resource resource;
  resource.set_dropped_attributes_count(42);
  KeyValue *attribute = resource.add_attributes();
  attribute->set_key("attribute_key");
  attribute->mutable_value()->set_int_value(42);

  std::string serialized_resource = resource.SerializePartialAsString();
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new(serialized_resource.c_str(), serialized_resource.length()));

  FilterXOtelResource *filterx_otel_resource = (FilterXOtelResource *) otel_resource_new(args);
  cr_assert(filterx_otel_resource);

  const opentelemetry::proto::resource::v1::Resource &resource_from_filterx = filterx_otel_resource->cpp->get_value();
  cr_assert(MessageDifferencer::Equals(resource, resource_from_filterx));

  filterx_object_unref(&filterx_otel_resource->super);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, resource_from_protobuf_invalid_arg)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("", 0));

  FilterXOtelResource *filterx_otel_resource = (FilterXOtelResource *) otel_resource_new(args);
  cr_assert_not(filterx_otel_resource);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, resource_from_protobuf_malformed_data)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new("1234", 4));

  FilterXOtelResource *filterx_otel_resource = (FilterXOtelResource *) otel_resource_new(args);
  cr_assert_not(filterx_otel_resource);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, resource_too_many_args)
{
  GPtrArray *args = g_ptr_array_new_full(2, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("foo", 3));
  g_ptr_array_insert(args, 1, filterx_protobuf_new("bar", 3));

  FilterXOtelResource *filterx_otel_resource = (FilterXOtelResource *) otel_resource_new(args);
  cr_assert_not(filterx_otel_resource);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, resource_get_field)
{
  opentelemetry::proto::resource::v1::Resource resource;
  resource.set_dropped_attributes_count(42);
  KeyValue *attribute_1 = resource.add_attributes();
  attribute_1->set_key("attribute_1");
  attribute_1->mutable_value()->set_int_value(1337);

  std::string serialized_resource = resource.SerializePartialAsString();
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new(serialized_resource.c_str(), serialized_resource.length()));

  FilterXObject *filterx_otel_resource = (FilterXObject *) otel_resource_new(args);
  cr_assert(filterx_otel_resource);

  _assert_filterx_integer_attribute(filterx_otel_resource, "dropped_attributes_count", 42);
  _assert_filterx_repeated_kv_attribute(filterx_otel_resource, "attributes", resource.attributes());

  FilterXObject *filterx_invalid = filterx_object_getattr(filterx_otel_resource, "invalid_attr");
  cr_assert_not(filterx_invalid);

  filterx_object_unref(filterx_otel_resource);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, resource_set_field)
{
  FilterXObject *filterx_otel_resource = (FilterXObject *) otel_resource_new(NULL);
  cr_assert(filterx_otel_resource);

  FilterXObject *filterx_integer = filterx_integer_new(42);
  cr_assert(filterx_object_setattr(filterx_otel_resource, "dropped_attributes_count", filterx_integer));

  KeyValueList attributes;
  KeyValue *attribute_1 = attributes.add_values();
  attribute_1->set_key("attribute_1");
  attribute_1->mutable_value()->set_int_value(1337);
  std::string serialized_attributes = attributes.SerializePartialAsString();
  GPtrArray *attributes_kvlist_args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(attributes_kvlist_args, 0, filterx_protobuf_new(serialized_attributes.c_str(),
                     serialized_attributes.length()));
  FilterXObject *filterx_kvlist = otel_kvlist_new(attributes_kvlist_args);
  cr_assert(filterx_object_setattr(filterx_otel_resource, "attributes", filterx_kvlist));

  cr_assert_not(filterx_object_setattr(filterx_otel_resource, "invalid_attr", filterx_integer));

  GString *serialized = g_string_new(NULL);
  LogMessageValueType type;
  cr_assert(filterx_object_marshal(filterx_otel_resource, serialized, &type));

  cr_assert_eq(type, LM_VT_PROTOBUF);

  opentelemetry::proto::resource::v1::Resource resource;
  cr_assert(resource.ParsePartialFromArray(serialized->str, serialized->len));
  cr_assert_eq(resource.dropped_attributes_count(), 42);

  g_string_free(serialized, TRUE);
  g_ptr_array_unref(attributes_kvlist_args);
  filterx_object_unref(filterx_integer);
  filterx_object_unref(filterx_otel_resource);
}


/* Scope */

Test(otel_filterx, scope_empty)
{
  FilterXOtelScope *filterx_otel_scope = (FilterXOtelScope *) otel_scope_new(NULL);
  cr_assert(filterx_otel_scope);

  cr_assert(MessageDifferencer::Equals(opentelemetry::proto::common::v1::InstrumentationScope(),
                                       filterx_otel_scope->cpp->get_value()));

  filterx_object_unref(&filterx_otel_scope->super);
}

Test(otel_filterx, scope_from_protobuf)
{
  opentelemetry::proto::common::v1::InstrumentationScope scope;
  scope.set_dropped_attributes_count(42);
  KeyValue *attribute = scope.add_attributes();
  scope.set_name("name");
  scope.set_version("version");
  attribute->set_key("attribute_key");
  attribute->mutable_value()->set_int_value(42);

  std::string serialized_scope = scope.SerializePartialAsString();
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new(serialized_scope.c_str(), serialized_scope.length()));

  FilterXOtelScope *filterx_otel_scope = (FilterXOtelScope *) otel_scope_new(args);
  cr_assert(filterx_otel_scope);

  const opentelemetry::proto::common::v1::InstrumentationScope &scope_from_filterx = filterx_otel_scope->cpp->get_value();
  cr_assert(MessageDifferencer::Equals(scope, scope_from_filterx));

  filterx_object_unref(&filterx_otel_scope->super);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, scope_from_protobuf_invalid_arg)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("", 0));

  FilterXOtelScope *filterx_otel_scope = (FilterXOtelScope *) otel_scope_new(args);
  cr_assert_not(filterx_otel_scope);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, scope_from_protobuf_malformed_data)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new("1234", 4));

  FilterXOtelScope *filterx_otel_scope = (FilterXOtelScope *) otel_scope_new(args);
  cr_assert_not(filterx_otel_scope);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, scope_too_many_args)
{
  GPtrArray *args = g_ptr_array_new_full(2, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("foo", 3));
  g_ptr_array_insert(args, 1, filterx_protobuf_new("bar", 3));

  FilterXOtelScope *filterx_otel_scope = (FilterXOtelScope *) otel_scope_new(args);
  cr_assert_not(filterx_otel_scope);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, scope_get_field)
{
  opentelemetry::proto::common::v1::InstrumentationScope scope;
  scope.set_dropped_attributes_count(42);
  scope.set_name("foobar");
  KeyValue *attribute_1 = scope.add_attributes();
  attribute_1->set_key("attribute_1");
  attribute_1->mutable_value()->set_int_value(1337);

  std::string serialized_scope = scope.SerializePartialAsString();
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new(serialized_scope.c_str(), serialized_scope.length()));

  FilterXObject *filterx_otel_scope = (FilterXObject *) otel_scope_new(args);
  cr_assert(filterx_otel_scope);

  _assert_filterx_integer_attribute(filterx_otel_scope, "dropped_attributes_count", 42);
  _assert_filterx_string_attribute(filterx_otel_scope, "name", "foobar");
  _assert_filterx_repeated_kv_attribute(filterx_otel_scope, "attributes", scope.attributes());

  FilterXObject *filterx_invalid = filterx_object_getattr(filterx_otel_scope, "invalid_attr");
  cr_assert_not(filterx_invalid);

  filterx_object_unref(filterx_otel_scope);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, scope_set_field)
{
  FilterXObject *filterx_otel_scope = (FilterXObject *) otel_scope_new(NULL);
  cr_assert(filterx_otel_scope);

  FilterXObject *filterx_integer = filterx_integer_new(42);
  cr_assert(filterx_object_setattr(filterx_otel_scope, "dropped_attributes_count", filterx_integer));

  FilterXObject *filterx_string = filterx_string_new("foobar", -1);
  cr_assert(filterx_object_setattr(filterx_otel_scope, "name", filterx_string));

  KeyValueList attributes;
  KeyValue *attribute_1 = attributes.add_values();
  attribute_1->set_key("attribute_1");
  attribute_1->mutable_value()->set_int_value(1337);
  std::string serialized_attributes = attributes.SerializePartialAsString();
  GPtrArray *attributes_kvlist_args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(attributes_kvlist_args, 0, filterx_protobuf_new(serialized_attributes.c_str(),
                     serialized_attributes.length()));
  FilterXObject *filterx_kvlist = otel_kvlist_new(attributes_kvlist_args);
  cr_assert(filterx_object_setattr(filterx_otel_scope, "attributes", filterx_kvlist));

  cr_assert_not(filterx_object_setattr(filterx_otel_scope, "invalid_attr", filterx_integer));

  GString *serialized = g_string_new(NULL);
  LogMessageValueType type;
  cr_assert(filterx_object_marshal(filterx_otel_scope, serialized, &type));

  cr_assert_eq(type, LM_VT_PROTOBUF);

  opentelemetry::proto::common::v1::InstrumentationScope scope;
  cr_assert(scope.ParsePartialFromArray(serialized->str, serialized->len));
  cr_assert_eq(scope.dropped_attributes_count(), 42);
  cr_assert_eq(scope.name().compare("foobar"), 0);

  g_string_free(serialized, TRUE);
  g_ptr_array_unref(attributes_kvlist_args);
  filterx_object_unref(filterx_kvlist);
  filterx_object_unref(filterx_string);
  filterx_object_unref(filterx_integer);
  filterx_object_unref(filterx_otel_scope);
}


/* KVList */

Test(otel_filterx, kvlist_empty)
{
  FilterXOtelKVList *filterx_otel_kvlist = (FilterXOtelKVList *) otel_kvlist_new(NULL);
  cr_assert(filterx_otel_kvlist);

  cr_assert(MessageDifferencer::Equals(opentelemetry::proto::common::v1::KeyValueList(),
                                       filterx_otel_kvlist->cpp->get_value()));

  filterx_object_unref(&filterx_otel_kvlist->super);
}

Test(otel_filterx, kvlist_from_protobuf)
{
  opentelemetry::proto::common::v1::KeyValueList kvlist;
  KeyValue *element_1 = kvlist.add_values();
  element_1->set_key("element_1_key");
  element_1->mutable_value()->set_int_value(42);
  KeyValue *element_2 = kvlist.add_values();
  element_2->set_key("element_2_key");
  element_2->mutable_value()->set_string_value("foobar");

  std::string serialized_kvlist = kvlist.SerializePartialAsString();
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new(serialized_kvlist.c_str(), serialized_kvlist.length()));

  FilterXOtelKVList *filterx_otel_kvlist = (FilterXOtelKVList *) otel_kvlist_new(args);
  cr_assert(filterx_otel_kvlist);

  const opentelemetry::proto::common::v1::KeyValueList &kvlist_from_filterx = filterx_otel_kvlist->cpp->get_value();
  cr_assert(MessageDifferencer::Equals(kvlist, kvlist_from_filterx));

  filterx_object_unref(&filterx_otel_kvlist->super);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, kvlist_from_protobuf_invalid_arg)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("", 0));

  FilterXOtelKVList *filterx_otel_kvlist = (FilterXOtelKVList *) otel_kvlist_new(args);
  cr_assert_not(filterx_otel_kvlist);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, kvlist_from_protobuf_malformed_data)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new("1234", 4));

  FilterXOtelKVList *filterx_otel_kvlist = (FilterXOtelKVList *) otel_kvlist_new(args);
  cr_assert_not(filterx_otel_kvlist);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, kvlist_too_many_args)
{
  GPtrArray *args = g_ptr_array_new_full(2, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("foo", 3));
  g_ptr_array_insert(args, 1, filterx_protobuf_new("bar", 3));

  FilterXOtelKVList *filterx_otel_kvlist = (FilterXOtelKVList *) otel_kvlist_new(args);
  cr_assert_not(filterx_otel_kvlist);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, kvlist_get_subscript)
{
  opentelemetry::proto::common::v1::KeyValueList kvlist;
  KeyValue *element_1 = kvlist.add_values();
  element_1->set_key("element_1_key");
  element_1->mutable_value()->set_int_value(42);
  KeyValue *element_2 = kvlist.add_values();
  element_2->set_key("element_2_key");
  element_2->mutable_value()->set_string_value("foobar");
  KeyValue *element_3 = kvlist.add_values();
  element_3->set_key("element_3_key");
  KeyValueList *inner_kvlist = element_3->mutable_value()->mutable_kvlist_value();
  KeyValue *inner_kv = inner_kvlist->add_values();
  inner_kv->set_key("inner_element");
  inner_kv->mutable_value()->set_int_value(1337);

  std::string serialized_kvlist = kvlist.SerializePartialAsString();
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new(serialized_kvlist.c_str(), serialized_kvlist.length()));

  FilterXObject *filterx_otel_kvlist = (FilterXObject *) otel_kvlist_new(args);
  cr_assert(filterx_otel_kvlist);

  FilterXObject *element_1_key = filterx_string_new("element_1_key", -1);
  FilterXObject *element_2_key = filterx_string_new("element_2_key", -1);
  FilterXObject *element_3_key = filterx_string_new("element_3_key", -1);
  _assert_filterx_integer_element(filterx_otel_kvlist, element_1_key, 42);
  _assert_filterx_string_element(filterx_otel_kvlist, element_2_key, "foobar");
  _assert_filterx_otel_kvlist_element(filterx_otel_kvlist, element_3_key, *inner_kvlist);
  // TODO: test array here as well

  FilterXObject *invalid_element_key = filterx_string_new("invalid_element_key", -1);
  FilterXObject *filterx_invalid = filterx_object_get_subscript(filterx_otel_kvlist, invalid_element_key);
  cr_assert_not(filterx_invalid);

  filterx_object_unref(invalid_element_key);
  filterx_object_unref(element_1_key);
  filterx_object_unref(element_2_key);
  filterx_object_unref(element_3_key);
  filterx_object_unref(filterx_otel_kvlist);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, kvlist_set_subscript)
{
  FilterXObject *filterx_otel_kvlist = (FilterXObject *) otel_kvlist_new(NULL);
  cr_assert(filterx_otel_kvlist);

  FilterXObject *element_1_key = filterx_string_new("element_1_key", -1);
  FilterXObject *element_1_value = filterx_integer_new(42);
  FilterXObject *element_2_key = filterx_string_new("element_2_key", -1);
  FilterXObject *element_2_value = filterx_string_new("foobar", -1);
  FilterXObject *element_3_key = filterx_string_new("element_3_key", -1);
  FilterXObject *element_3_value = otel_kvlist_new(NULL);
  cr_assert(filterx_object_set_subscript(filterx_otel_kvlist, element_1_key, element_1_value));
  cr_assert(filterx_object_set_subscript(filterx_otel_kvlist, element_2_key, element_2_value));
  cr_assert(filterx_object_set_subscript(filterx_otel_kvlist, element_3_key, element_3_value));
  // TODO: test array here as well

  FilterXObject *invalid_element_key = filterx_integer_new(1234);
  cr_assert_not(filterx_object_set_subscript(filterx_otel_kvlist, invalid_element_key, element_1_value));

  GString *serialized = g_string_new(NULL);
  LogMessageValueType type;
  cr_assert(filterx_object_marshal(filterx_otel_kvlist, serialized, &type));

  cr_assert_eq(type, LM_VT_PROTOBUF);

  opentelemetry::proto::common::v1::KeyValueList kvlist;
  cr_assert(kvlist.ParsePartialFromArray(serialized->str, serialized->len));
  cr_assert_eq(kvlist.values_size(), 3);
  cr_assert_eq(kvlist.values(0).value().int_value(), 42);
  cr_assert_eq(kvlist.values(1).value().string_value().compare("foobar"), 0);
  cr_assert_eq(kvlist.values(2).value().value_case(), AnyValue::kKvlistValue);
  cr_assert(MessageDifferencer::Equals(kvlist.values(2).value().kvlist_value(), KeyValueList()));

  g_string_free(serialized, TRUE);
  filterx_object_unref(element_1_key);
  filterx_object_unref(element_1_value);
  filterx_object_unref(element_2_key);
  filterx_object_unref(element_2_value);
  filterx_object_unref(element_3_key);
  filterx_object_unref(element_3_value);
  filterx_object_unref(invalid_element_key);
  filterx_object_unref(filterx_otel_kvlist);
}

void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
}

void
teardown(void)
{
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(otel_filterx, .init = setup, .fini = teardown);
