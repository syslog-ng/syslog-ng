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
#include "filterx/object-otel-array.hpp"

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
  FilterXObject *filterx_integer = filterx_object_getattr_string(obj, attribute_name.c_str());
  cr_assert(filterx_object_is_type(filterx_integer, &FILTERX_TYPE_NAME(integer)));

  GenericNumber value = filterx_primitive_get_value(filterx_integer);
  cr_assert_eq(gn_as_int64(&value), expected_value);

  filterx_object_unref(filterx_integer);
}

static void
_assert_filterx_string_attribute(FilterXObject *obj, const std::string &attribute_name,
                                 const std::string &expected_value)
{
  FilterXObject *filterx_string = filterx_object_getattr_string(obj, attribute_name.c_str());
  cr_assert(filterx_object_is_type(filterx_string, &FILTERX_TYPE_NAME(string)));

  gsize len;
  const gchar *value = filterx_string_get_value(filterx_string, &len);
  cr_assert_eq(expected_value.compare(std::string(value, len)), 0);

  filterx_object_unref(filterx_string);
}

static void
_assert_repeated_kvs(const google::protobuf::RepeatedPtrField<KeyValue> &repeated_kv,
                     const google::protobuf::RepeatedPtrField<KeyValue> &expected_repeated_kv)
{
  cr_assert_eq(expected_repeated_kv.size(), repeated_kv.size());
  for (int i = 0; i < repeated_kv.size(); i++)
    {
      cr_assert(MessageDifferencer::Equals(repeated_kv.at(i), expected_repeated_kv.at(i)));
    }
}

static void
_assert_filterx_repeated_kv_attribute(FilterXObject *obj, const std::string &attribute_name,
                                      const google::protobuf::RepeatedPtrField<KeyValue> &expected_repeated_kv)
{
  FilterXObject *filterx_otel_kvlist = filterx_object_getattr_string(obj, attribute_name.c_str());
  cr_assert(filterx_object_is_type(filterx_otel_kvlist, &FILTERX_TYPE_NAME(otel_kvlist)));

  const google::protobuf::RepeatedPtrField<KeyValue> &kvlist = ((FilterXOtelKVList *)
      filterx_otel_kvlist)->cpp->get_value();
  _assert_repeated_kvs(kvlist, expected_repeated_kv);

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

  _assert_repeated_kvs(((FilterXOtelKVList *) filterx_otel_kvlist)->cpp->get_value(), expected_otel_kvlist.values());

  filterx_object_unref(filterx_otel_kvlist);
}

static void
_assert_filterx_otel_array_element(FilterXObject *obj, FilterXObject *key,
                                   const ArrayValue &expected_otel_array)
{
  FilterXObject *filterx_otel_array = filterx_object_get_subscript(obj, key);
  cr_assert(filterx_otel_array);
  cr_assert(filterx_object_is_type(filterx_otel_array, &FILTERX_TYPE_NAME(otel_array)));

  const ArrayValue &array = ((FilterXOtelArray *) filterx_otel_array)->cpp->get_value();
  cr_assert(MessageDifferencer::Equals(array, expected_otel_array));

  filterx_object_unref(filterx_otel_array);
}


/* LogRecord */

Test(otel_filterx, logrecord_empty)
{
  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) filterx_otel_logrecord_new_from_args(NULL);
  cr_assert(filterx_otel_logrecord);

  cr_assert(MessageDifferencer::Equals(LogRecord(), filterx_otel_logrecord->cpp->get_value()));

  filterx_object_unref(&filterx_otel_logrecord->super.super);
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

  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) filterx_otel_logrecord_new_from_args(args);
  cr_assert(filterx_otel_logrecord);

  const LogRecord &log_record_from_filterx = filterx_otel_logrecord->cpp->get_value();
  cr_assert(MessageDifferencer::Equals(log_record, log_record_from_filterx));

  filterx_object_unref(&filterx_otel_logrecord->super.super);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, logrecord_from_protobuf_invalid_arg)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("", 0));

  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) filterx_otel_logrecord_new_from_args(args);
  cr_assert_not(filterx_otel_logrecord);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, logrecord_from_protobuf_malformed_data)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new("1234", 4));

  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) filterx_otel_logrecord_new_from_args(args);
  cr_assert_not(filterx_otel_logrecord);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, logrecord_too_many_args)
{
  GPtrArray *args = g_ptr_array_new_full(2, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("foo", 3));
  g_ptr_array_insert(args, 1, filterx_protobuf_new("bar", 3));

  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) filterx_otel_logrecord_new_from_args(args);
  cr_assert_not(filterx_otel_logrecord);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, logrecord_len_and_unset_and_is_key_set)
{
  FilterXObject *logrecord = filterx_otel_logrecord_new_from_args(NULL);
  FilterXObject *body = filterx_string_new("body", -1);
  FilterXObject *body_val = filterx_string_new("body_val", -1);
  FilterXObject *time_unix_nano = filterx_string_new("time_unix_nano", -1);
  FilterXObject *time_unix_nano_val = filterx_integer_new(123);

  guint64 len;
  cr_assert(filterx_object_len(logrecord, &len));
  cr_assert_eq(len, 0);

  cr_assert_not(filterx_object_is_key_set(logrecord, body));
  cr_assert(filterx_object_set_subscript(logrecord, body, &body_val));
  cr_assert(filterx_object_len(logrecord, &len));
  cr_assert_eq(len, 1);
  cr_assert(filterx_object_is_key_set(logrecord, body));

  cr_assert_not(filterx_object_is_key_set(logrecord, time_unix_nano));
  cr_assert(filterx_object_set_subscript(logrecord, time_unix_nano, &time_unix_nano_val));
  cr_assert(filterx_object_len(logrecord, &len));
  cr_assert_eq(len, 2);
  cr_assert(filterx_object_is_key_set(logrecord, time_unix_nano));

  cr_assert(filterx_object_unset_key(logrecord, body));
  cr_assert(filterx_object_len(logrecord, &len));
  cr_assert_eq(len, 1);
  cr_assert_not(filterx_object_is_key_set(logrecord, body));

  cr_assert(filterx_object_unset_key(logrecord, time_unix_nano));
  cr_assert(filterx_object_len(logrecord, &len));
  cr_assert_eq(len, 0);
  cr_assert_not(filterx_object_is_key_set(logrecord, time_unix_nano));

  filterx_object_unref(time_unix_nano);
  filterx_object_unref(time_unix_nano_val);
  filterx_object_unref(body);
  filterx_object_unref(body_val);
  filterx_object_unref(logrecord);
}

static gboolean
_append_to_str(FilterXObject *key, FilterXObject *value, gpointer user_data)
{
  GString *output = (GString *) user_data;

  LogMessageValueType type;
  filterx_object_marshal_append(key, output, &type);
  g_string_append_c(output, '\n');
  filterx_object_marshal_append(value, output, &type);
  g_string_append_c(output, '\n');

  return TRUE;
}

Test(otel_filterx, logrecord_iter)
{
  FilterXObject *logrecord = filterx_otel_logrecord_new_from_args(NULL);
  FilterXObject *body = filterx_string_new("body", -1);
  FilterXObject *body_val = filterx_string_new("body_val", -1);
  FilterXObject *time_unix_nano = filterx_string_new("time_unix_nano", -1);
  FilterXObject *time_unix_nano_val = filterx_integer_new(123);

  cr_assert(filterx_object_set_subscript(logrecord, body, &body_val));
  cr_assert(filterx_object_set_subscript(logrecord, time_unix_nano, &time_unix_nano_val));

  GString *output = g_string_new(NULL);
  cr_assert(filterx_dict_iter(logrecord, _append_to_str, output));

  cr_assert_str_eq(output->str, "time_unix_nano\n0.000123+00:00\nbody\nbody_val\n");

  g_string_free(output, TRUE);
  filterx_object_unref(time_unix_nano);
  filterx_object_unref(time_unix_nano_val);
  filterx_object_unref(body);
  filterx_object_unref(body_val);
  filterx_object_unref(logrecord);
}


/* Resource */

Test(otel_filterx, resource_empty)
{
  FilterXOtelResource *filterx_otel_resource = (FilterXOtelResource *) filterx_otel_resource_new_from_args(NULL);
  cr_assert(filterx_otel_resource);

  cr_assert(MessageDifferencer::Equals(opentelemetry::proto::resource::v1::Resource(),
                                       filterx_otel_resource->cpp->get_value()));

  filterx_object_unref(&filterx_otel_resource->super.super);
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

  FilterXOtelResource *filterx_otel_resource = (FilterXOtelResource *) filterx_otel_resource_new_from_args(args);
  cr_assert(filterx_otel_resource);

  const opentelemetry::proto::resource::v1::Resource &resource_from_filterx = filterx_otel_resource->cpp->get_value();
  cr_assert(MessageDifferencer::Equals(resource, resource_from_filterx));

  filterx_object_unref(&filterx_otel_resource->super.super);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, resource_from_protobuf_invalid_arg)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("", 0));

  FilterXOtelResource *filterx_otel_resource = (FilterXOtelResource *) filterx_otel_resource_new_from_args(args);
  cr_assert_not(filterx_otel_resource);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, resource_from_protobuf_malformed_data)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new("1234", 4));

  FilterXOtelResource *filterx_otel_resource = (FilterXOtelResource *) filterx_otel_resource_new_from_args(args);
  cr_assert_not(filterx_otel_resource);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, resource_too_many_args)
{
  GPtrArray *args = g_ptr_array_new_full(2, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("foo", 3));
  g_ptr_array_insert(args, 1, filterx_protobuf_new("bar", 3));

  FilterXOtelResource *filterx_otel_resource = (FilterXOtelResource *) filterx_otel_resource_new_from_args(args);
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

  FilterXObject *filterx_otel_resource = (FilterXObject *) filterx_otel_resource_new_from_args(args);
  cr_assert(filterx_otel_resource);

  _assert_filterx_integer_attribute(filterx_otel_resource, "dropped_attributes_count", 42);
  _assert_filterx_repeated_kv_attribute(filterx_otel_resource, "attributes", resource.attributes());

  FilterXObject *filterx_invalid = filterx_object_getattr_string(filterx_otel_resource, "invalid_attr");
  cr_assert_not(filterx_invalid);

  filterx_object_unref(filterx_otel_resource);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, resource_set_field)
{
  FilterXObject *filterx_otel_resource = (FilterXObject *) filterx_otel_resource_new_from_args(NULL);
  cr_assert(filterx_otel_resource);

  FilterXObject *filterx_integer = filterx_integer_new(42);
  cr_assert(filterx_object_setattr_string(filterx_otel_resource, "dropped_attributes_count", &filterx_integer));

  KeyValueList attributes;
  KeyValue *attribute_1 = attributes.add_values();
  attribute_1->set_key("attribute_1");
  attribute_1->mutable_value()->set_int_value(1337);
  std::string serialized_attributes = attributes.SerializePartialAsString();
  GPtrArray *attributes_kvlist_args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(attributes_kvlist_args, 0, filterx_protobuf_new(serialized_attributes.c_str(),
                     serialized_attributes.length()));
  FilterXObject *filterx_kvlist = filterx_otel_kvlist_new_from_args(attributes_kvlist_args);
  cr_assert(filterx_object_setattr_string(filterx_otel_resource, "attributes", &filterx_kvlist));

  cr_assert_not(filterx_object_setattr_string(filterx_otel_resource, "invalid_attr", &filterx_integer));

  GString *serialized = g_string_new(NULL);
  LogMessageValueType type;
  cr_assert(filterx_object_marshal(filterx_otel_resource, serialized, &type));

  cr_assert_eq(type, LM_VT_PROTOBUF);

  opentelemetry::proto::resource::v1::Resource resource;
  cr_assert(resource.ParsePartialFromArray(serialized->str, serialized->len));
  cr_assert_eq(resource.dropped_attributes_count(), 42);

  g_string_free(serialized, TRUE);
  g_ptr_array_unref(attributes_kvlist_args);
  filterx_object_unref(filterx_kvlist);
  filterx_object_unref(filterx_integer);
  filterx_object_unref(filterx_otel_resource);
}

Test(otel_filterx, resource_len_and_unset_and_is_key_set)
{
  FilterXObject *resource = filterx_otel_resource_new_from_args(NULL);
  FilterXObject *dropped_attributes_count = filterx_string_new("dropped_attributes_count", -1);
  FilterXObject *dropped_attributes_count_val = filterx_integer_new(42);

  guint64 len;
  cr_assert(filterx_object_len(resource, &len));
  cr_assert_eq(len, 0);

  cr_assert_not(filterx_object_is_key_set(resource, dropped_attributes_count));
  cr_assert(filterx_object_set_subscript(resource, dropped_attributes_count, &dropped_attributes_count_val));
  cr_assert(filterx_object_len(resource, &len));
  cr_assert_eq(len, 1);
  cr_assert(filterx_object_is_key_set(resource, dropped_attributes_count));

  cr_assert(filterx_object_unset_key(resource, dropped_attributes_count));
  cr_assert(filterx_object_len(resource, &len));
  cr_assert_eq(len, 0);
  cr_assert_not(filterx_object_is_key_set(resource, dropped_attributes_count));

  filterx_object_unref(dropped_attributes_count);
  filterx_object_unref(dropped_attributes_count_val);
  filterx_object_unref(resource);
}

Test(otel_filterx, resource_iter)
{
  FilterXObject *resource = filterx_otel_resource_new_from_args(NULL);
  FilterXObject *dropped_attributes_count = filterx_string_new("dropped_attributes_count", -1);
  FilterXObject *dropped_attributes_count_val = filterx_integer_new(42);

  cr_assert(filterx_object_set_subscript(resource, dropped_attributes_count, &dropped_attributes_count_val));

  GString *output = g_string_new(NULL);
  cr_assert(filterx_dict_iter(resource, _append_to_str, output));

  cr_assert_str_eq(output->str, "dropped_attributes_count\n42\n");

  g_string_free(output, TRUE);
  filterx_object_unref(dropped_attributes_count);
  filterx_object_unref(dropped_attributes_count_val);
  filterx_object_unref(resource);
}


/* Scope */

Test(otel_filterx, scope_empty)
{
  FilterXOtelScope *filterx_otel_scope = (FilterXOtelScope *) filterx_otel_scope_new_from_args(NULL);
  cr_assert(filterx_otel_scope);

  cr_assert(MessageDifferencer::Equals(opentelemetry::proto::common::v1::InstrumentationScope(),
                                       filterx_otel_scope->cpp->get_value()));

  filterx_object_unref(&filterx_otel_scope->super.super);
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

  FilterXOtelScope *filterx_otel_scope = (FilterXOtelScope *) filterx_otel_scope_new_from_args(args);
  cr_assert(filterx_otel_scope);

  const opentelemetry::proto::common::v1::InstrumentationScope &scope_from_filterx = filterx_otel_scope->cpp->get_value();
  cr_assert(MessageDifferencer::Equals(scope, scope_from_filterx));

  filterx_object_unref(&filterx_otel_scope->super.super);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, scope_from_protobuf_invalid_arg)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("", 0));

  FilterXOtelScope *filterx_otel_scope = (FilterXOtelScope *) filterx_otel_scope_new_from_args(args);
  cr_assert_not(filterx_otel_scope);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, scope_from_protobuf_malformed_data)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new("1234", 4));

  FilterXOtelScope *filterx_otel_scope = (FilterXOtelScope *) filterx_otel_scope_new_from_args(args);
  cr_assert_not(filterx_otel_scope);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, scope_too_many_args)
{
  GPtrArray *args = g_ptr_array_new_full(2, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("foo", 3));
  g_ptr_array_insert(args, 1, filterx_protobuf_new("bar", 3));

  FilterXOtelScope *filterx_otel_scope = (FilterXOtelScope *) filterx_otel_scope_new_from_args(args);
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

  FilterXObject *filterx_otel_scope = (FilterXObject *) filterx_otel_scope_new_from_args(args);
  cr_assert(filterx_otel_scope);

  _assert_filterx_integer_attribute(filterx_otel_scope, "dropped_attributes_count", 42);
  _assert_filterx_string_attribute(filterx_otel_scope, "name", "foobar");
  _assert_filterx_repeated_kv_attribute(filterx_otel_scope, "attributes", scope.attributes());

  FilterXObject *filterx_invalid = filterx_object_getattr_string(filterx_otel_scope, "invalid_attr");
  cr_assert_not(filterx_invalid);

  filterx_object_unref(filterx_otel_scope);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, scope_set_field)
{
  FilterXObject *filterx_otel_scope = (FilterXObject *) filterx_otel_scope_new_from_args(NULL);
  cr_assert(filterx_otel_scope);

  FilterXObject *filterx_integer = filterx_integer_new(42);
  cr_assert(filterx_object_setattr_string(filterx_otel_scope, "dropped_attributes_count", &filterx_integer));

  FilterXObject *filterx_string = filterx_string_new("foobar", -1);
  cr_assert(filterx_object_setattr_string(filterx_otel_scope, "name", &filterx_string));

  KeyValueList attributes;
  KeyValue *attribute_1 = attributes.add_values();
  attribute_1->set_key("attribute_1");
  attribute_1->mutable_value()->set_int_value(1337);
  std::string serialized_attributes = attributes.SerializePartialAsString();
  GPtrArray *attributes_kvlist_args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(attributes_kvlist_args, 0, filterx_protobuf_new(serialized_attributes.c_str(),
                     serialized_attributes.length()));
  FilterXObject *filterx_kvlist = filterx_otel_kvlist_new_from_args(attributes_kvlist_args);
  cr_assert(filterx_object_setattr_string(filterx_otel_scope, "attributes", &filterx_kvlist));

  cr_assert_not(filterx_object_setattr_string(filterx_otel_scope, "invalid_attr", &filterx_integer));

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
  FilterXOtelKVList *filterx_otel_kvlist = (FilterXOtelKVList *) filterx_otel_kvlist_new_from_args(NULL);
  cr_assert(filterx_otel_kvlist);

  cr_assert_eq(filterx_otel_kvlist->cpp->get_value().size(), 0);

  filterx_object_unref(&filterx_otel_kvlist->super.super);
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

  FilterXOtelKVList *filterx_otel_kvlist = (FilterXOtelKVList *) filterx_otel_kvlist_new_from_args(args);
  cr_assert(filterx_otel_kvlist);

  _assert_repeated_kvs(filterx_otel_kvlist->cpp->get_value(), kvlist.values());

  filterx_object_unref(&filterx_otel_kvlist->super.super);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, kvlist_from_protobuf_invalid_arg)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("", 0));

  FilterXOtelKVList *filterx_otel_kvlist = (FilterXOtelKVList *) filterx_otel_kvlist_new_from_args(args);
  cr_assert_not(filterx_otel_kvlist);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, kvlist_from_protobuf_malformed_data)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new("1234", 4));

  FilterXOtelKVList *filterx_otel_kvlist = (FilterXOtelKVList *) filterx_otel_kvlist_new_from_args(args);
  cr_assert_not(filterx_otel_kvlist);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, kvlist_too_many_args)
{
  GPtrArray *args = g_ptr_array_new_full(2, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("foo", 3));
  g_ptr_array_insert(args, 1, filterx_protobuf_new("bar", 3));

  FilterXOtelKVList *filterx_otel_kvlist = (FilterXOtelKVList *) filterx_otel_kvlist_new_from_args(args);
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
  KeyValue *element_4 = kvlist.add_values();
  element_4->set_key("element_4_key");
  ArrayValue *inner_array = element_4->mutable_value()->mutable_array_value();
  inner_array->add_values()->set_string_value("inner-string");
  inner_array->add_values()->set_int_value(1000);

  std::string serialized_kvlist = kvlist.SerializePartialAsString();
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new(serialized_kvlist.c_str(), serialized_kvlist.length()));

  FilterXObject *filterx_otel_kvlist = (FilterXObject *) filterx_otel_kvlist_new_from_args(args);
  cr_assert(filterx_otel_kvlist);

  FilterXObject *element_1_key = filterx_string_new("element_1_key", -1);
  FilterXObject *element_2_key = filterx_string_new("element_2_key", -1);
  FilterXObject *element_3_key = filterx_string_new("element_3_key", -1);
  FilterXObject *element_4_key = filterx_string_new("element_4_key", -1);
  _assert_filterx_integer_element(filterx_otel_kvlist, element_1_key, 42);
  _assert_filterx_string_element(filterx_otel_kvlist, element_2_key, "foobar");
  _assert_filterx_otel_kvlist_element(filterx_otel_kvlist, element_3_key, *inner_kvlist);
  _assert_filterx_otel_array_element(filterx_otel_kvlist, element_4_key, *inner_array);

  filterx_object_unref(element_1_key);
  filterx_object_unref(element_2_key);
  filterx_object_unref(element_3_key);
  filterx_object_unref(element_4_key);
  filterx_object_unref(filterx_otel_kvlist);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, kvlist_set_subscript)
{
  FilterXObject *filterx_otel_kvlist = (FilterXObject *) filterx_otel_kvlist_new_from_args(NULL);
  cr_assert(filterx_otel_kvlist);

  FilterXObject *element_1_key = filterx_string_new("element_1_key", -1);
  FilterXObject *element_1_value = filterx_integer_new(42);
  FilterXObject *element_2_key = filterx_string_new("element_2_key", -1);
  FilterXObject *element_2_value = filterx_string_new("foobar", -1);
  FilterXObject *element_3_key = filterx_string_new("element_3_key", -1);
  FilterXObject *element_3_value = filterx_otel_kvlist_new_from_args(NULL);
  FilterXObject *element_4_key = filterx_string_new("element_4_key", -1);
  FilterXObject *element_4_value = filterx_otel_array_new_from_args(NULL);
  cr_assert(filterx_object_set_subscript(filterx_otel_kvlist, element_1_key, &element_1_value));
  cr_assert(filterx_object_set_subscript(filterx_otel_kvlist, element_2_key, &element_2_value));
  cr_assert(filterx_object_set_subscript(filterx_otel_kvlist, element_3_key, &element_3_value));
  cr_assert(filterx_object_set_subscript(filterx_otel_kvlist, element_4_key, &element_4_value));

  FilterXObject *invalid_element_key = filterx_integer_new(1234);
  cr_assert_not(filterx_object_set_subscript(filterx_otel_kvlist, invalid_element_key, &element_1_value));

  GString *serialized = g_string_new(NULL);
  LogMessageValueType type;
  cr_assert(filterx_object_marshal(filterx_otel_kvlist, serialized, &type));

  cr_assert_eq(type, LM_VT_PROTOBUF);

  opentelemetry::proto::common::v1::KeyValueList kvlist;
  cr_assert(kvlist.ParsePartialFromArray(serialized->str, serialized->len));
  cr_assert_eq(kvlist.values_size(), 4);
  cr_assert_eq(kvlist.values(0).value().int_value(), 42);
  cr_assert_eq(kvlist.values(1).value().string_value().compare("foobar"), 0);
  cr_assert_eq(kvlist.values(2).value().value_case(), AnyValue::kKvlistValue);
  cr_assert(MessageDifferencer::Equals(kvlist.values(2).value().kvlist_value(), KeyValueList()));
  cr_assert_eq(kvlist.values(3).value().value_case(), AnyValue::kArrayValue);
  cr_assert(MessageDifferencer::Equals(kvlist.values(3).value().array_value(), ArrayValue()));

  g_string_free(serialized, TRUE);
  filterx_object_unref(element_1_key);
  filterx_object_unref(element_1_value);
  filterx_object_unref(element_2_key);
  filterx_object_unref(element_2_value);
  filterx_object_unref(element_3_key);
  filterx_object_unref(element_3_value);
  filterx_object_unref(element_4_key);
  filterx_object_unref(element_4_value);
  filterx_object_unref(invalid_element_key);
  filterx_object_unref(filterx_otel_kvlist);
}

Test(otel_filterx, kvlist_through_logrecord)
{
  FilterXObject *fx_logrecord = filterx_otel_logrecord_new_from_args(NULL);
  FilterXObject *fx_kvlist = filterx_otel_kvlist_new_from_args(NULL);

  FilterXObject *fx_key_0 = filterx_string_new("key_0", -1);
  FilterXObject *fx_key_1 = filterx_string_new("key_1", -1);
  FilterXObject *fx_key_2 = filterx_string_new("key_2", -1);
  FilterXObject *fx_key_3 = filterx_string_new("key_3", -1);

  FilterXObject *fx_foo = filterx_string_new("foo", -1);
  FilterXObject *fx_bar = filterx_string_new("bar", -1);
  FilterXObject *fx_baz = filterx_string_new("baz", -1);

  FilterXObject *fx_inner_kvlist = filterx_otel_kvlist_new_from_args(NULL);

  /* objects for storing the result of getattr() and get_subscript() */
  FilterXObject *fx_get_1 = nullptr;
  FilterXObject *fx_get_2 = nullptr;

  /* $kvlist["key_0"] = "foo"; */
  cr_assert(filterx_object_set_subscript(fx_kvlist, fx_key_0, &fx_foo));

  /* $log.attributes = $kvlist; */
  FilterXObject *fx_kvlist_clone = filterx_object_clone(fx_kvlist);
  cr_assert(filterx_object_setattr_string(fx_logrecord, "attributes", &fx_kvlist_clone));
  filterx_object_unref(fx_kvlist_clone);

  /* $log.attributes["key_1"] = "bar"; */
  fx_get_1 = filterx_object_getattr_string(fx_logrecord, "attributes");
  cr_assert(fx_get_1);
  cr_assert(filterx_object_set_subscript(fx_get_1, fx_key_1, &fx_bar));

  /* $kvlist["key_2"] = "baz"; */
  cr_assert(filterx_object_set_subscript(fx_kvlist, fx_key_2, &fx_baz));

  /* $log.attributes["key_3"] = $inner_kvlist; */
  filterx_object_unref(fx_get_1);
  fx_get_1 = filterx_object_getattr_string(fx_logrecord, "attributes");
  cr_assert(fx_get_1);
  FilterXObject *fx_inner_kvlist_clone = filterx_object_clone(fx_inner_kvlist);
  cr_assert(filterx_object_set_subscript(fx_get_1, fx_key_3, &fx_inner_kvlist_clone));
  filterx_object_unref(fx_inner_kvlist_clone);

  /* $inner_kvlist["key_0"] = "foo"; */
  cr_assert(filterx_object_set_subscript(fx_inner_kvlist, fx_key_0, &fx_foo));

  /* $log.attributes["key_3"]["key_2"] = "baz"; */
  filterx_object_unref(fx_get_1);
  fx_get_1 = filterx_object_getattr_string(fx_logrecord, "attributes");
  cr_assert(fx_get_1);
  filterx_object_unref(fx_get_2);
  fx_get_2 = filterx_object_get_subscript(fx_get_1, fx_key_3);
  cr_assert(fx_get_2);
  cr_assert(filterx_object_set_subscript(fx_get_2, fx_key_2, &fx_baz));

  LogRecord expected_logrecord;
  KeyValue *expected_logrecord_attr_0 = expected_logrecord.add_attributes();
  expected_logrecord_attr_0->set_key("key_0");
  expected_logrecord_attr_0->mutable_value()->set_string_value("foo");
  KeyValue *expected_logrecord_attr_1 = expected_logrecord.add_attributes();
  expected_logrecord_attr_1->set_key("key_1");
  expected_logrecord_attr_1->mutable_value()->set_string_value("bar");
  KeyValue *expected_logrecord_attr_3 = expected_logrecord.add_attributes();
  expected_logrecord_attr_3->set_key("key_3");
  KeyValueList *expected_logrecord_inner_kvlist = expected_logrecord_attr_3->mutable_value()->mutable_kvlist_value();
  KeyValue *expected_logrecord_inner_attr_2 = expected_logrecord_inner_kvlist->add_values();
  expected_logrecord_inner_attr_2->set_key("key_2");
  expected_logrecord_inner_attr_2->mutable_value()->set_string_value("baz");
  cr_assert(MessageDifferencer::Equals(expected_logrecord, ((FilterXOtelLogRecord *) fx_logrecord)->cpp->get_value()));

  KeyValueList expected_kvlist;
  KeyValue *expected_kvlist_value_0 = expected_kvlist.add_values();
  expected_kvlist_value_0->set_key("key_0");
  expected_kvlist_value_0->mutable_value()->set_string_value("foo");
  KeyValue *expected_kvlist_value_2 = expected_kvlist.add_values();
  expected_kvlist_value_2->set_key("key_2");
  expected_kvlist_value_2->mutable_value()->set_string_value("baz");
  _assert_repeated_kvs(expected_kvlist.values(), ((FilterXOtelKVList *) fx_kvlist)->cpp->get_value());

  KeyValueList expected_inner_kvlist;
  KeyValue *expected_inner_kvlist_value_0 = expected_inner_kvlist.add_values();
  expected_inner_kvlist_value_0->set_key("key_0");
  expected_inner_kvlist_value_0->mutable_value()->set_string_value("foo");
  _assert_repeated_kvs(expected_inner_kvlist.values(), ((FilterXOtelKVList *) fx_inner_kvlist)->cpp->get_value());

  filterx_object_unref(fx_get_2);
  filterx_object_unref(fx_get_1);
  filterx_object_unref(fx_inner_kvlist);
  filterx_object_unref(fx_baz);
  filterx_object_unref(fx_bar);
  filterx_object_unref(fx_foo);
  filterx_object_unref(fx_key_3);
  filterx_object_unref(fx_key_2);
  filterx_object_unref(fx_key_1);
  filterx_object_unref(fx_key_0);
  filterx_object_unref(fx_kvlist);
  filterx_object_unref(fx_logrecord);
}

/* Array */

Test(otel_filterx, array_empty)
{
  FilterXOtelArray *filterx_otel_kvlist = (FilterXOtelArray *) filterx_otel_array_new_from_args(NULL);
  cr_assert(filterx_otel_kvlist);

  cr_assert(MessageDifferencer::Equals(opentelemetry::proto::common::v1::ArrayValue(),
                                       filterx_otel_kvlist->cpp->get_value()));

  filterx_object_unref(&filterx_otel_kvlist->super.super);
}

Test(otel_filterx, array_from_protobuf)
{
  opentelemetry::proto::common::v1::ArrayValue array;
  AnyValue *element_1 = array.add_values();
  element_1->set_int_value(42);
  AnyValue *element_2 = array.add_values();
  element_2->set_string_value("foobar");

  std::string serialized_array = array.SerializePartialAsString();
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new(serialized_array.c_str(), serialized_array.length()));

  FilterXOtelArray *filterx_otel_array = (FilterXOtelArray *) filterx_otel_array_new_from_args(args);
  cr_assert(filterx_otel_array);

  const opentelemetry::proto::common::v1::ArrayValue &array_from_filterx = filterx_otel_array->cpp->get_value();
  cr_assert(MessageDifferencer::Equals(array, array_from_filterx));

  filterx_object_unref(&filterx_otel_array->super.super);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, array_from_protobuf_invalid_arg)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("", 0));

  FilterXOtelArray *filterx_otel_array = (FilterXOtelArray *) filterx_otel_array_new_from_args(args);
  cr_assert_not(filterx_otel_array);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, array_from_protobuf_malformed_data)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new("1234", 4));

  FilterXOtelArray *filterx_otel_array = (FilterXOtelArray *) filterx_otel_array_new_from_args(args);
  cr_assert_not(filterx_otel_array);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, array_too_many_args)
{
  GPtrArray *args = g_ptr_array_new_full(2, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("foo", 3));
  g_ptr_array_insert(args, 1, filterx_protobuf_new("bar", 3));

  FilterXOtelArray *filterx_otel_array = (FilterXOtelArray *) filterx_otel_array_new_from_args(args);
  cr_assert_not(filterx_otel_array);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, array_get_subscript)
{
  opentelemetry::proto::common::v1::ArrayValue array;
  AnyValue *element_1 = array.add_values();
  element_1->set_int_value(42);
  AnyValue *element_2 = array.add_values();
  element_2->set_string_value("foobar");
  AnyValue *element_3 = array.add_values();
  KeyValueList *inner_kvlist = element_3->mutable_kvlist_value();
  KeyValue *inner_kv = inner_kvlist->add_values();
  inner_kv->set_key("inner_element");
  inner_kv->mutable_value()->set_int_value(1337);
  AnyValue *element_4 = array.add_values();
  ArrayValue *inner_array = element_4->mutable_array_value();
  inner_array->add_values()->set_string_value("inner-string");
  inner_array->add_values()->set_int_value(1000);

  std::string serialized_array = array.SerializePartialAsString();
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new(serialized_array.c_str(), serialized_array.length()));

  FilterXObject *filterx_otel_array = (FilterXObject *) filterx_otel_array_new_from_args(args);
  cr_assert(filterx_otel_array);

  FilterXObject *element_1_index = filterx_integer_new(0);
  FilterXObject *element_2_index = filterx_integer_new(1);
  FilterXObject *element_3_index = filterx_integer_new(2);
  FilterXObject *element_4_index = filterx_integer_new(3);
  _assert_filterx_integer_element(filterx_otel_array, element_1_index, 42);
  _assert_filterx_string_element(filterx_otel_array, element_2_index, "foobar");
  _assert_filterx_otel_kvlist_element(filterx_otel_array, element_3_index, *inner_kvlist);
  _assert_filterx_otel_array_element(filterx_otel_array, element_4_index, *inner_array);

  FilterXObject *invalid_element_key = filterx_string_new("invalid_element_key", -1);
  FilterXObject *filterx_invalid = filterx_object_get_subscript(filterx_otel_array, invalid_element_key);
  cr_assert_not(filterx_invalid);

  filterx_object_unref(invalid_element_key);
  filterx_object_unref(element_1_index);
  filterx_object_unref(element_2_index);
  filterx_object_unref(element_3_index);
  filterx_object_unref(element_4_index);
  filterx_object_unref(filterx_otel_array);
  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, array_set_subscript)
{
  FilterXObject *filterx_otel_array = (FilterXObject *) filterx_otel_array_new_from_args(NULL);
  cr_assert(filterx_otel_array);

  FilterXObject *element_1_value = filterx_integer_new(42);
  FilterXObject *element_2_value = filterx_string_new("foobar", -1);
  FilterXObject *element_3_value = filterx_otel_kvlist_new_from_args(NULL);
  FilterXObject *element_4_value = filterx_otel_array_new_from_args(NULL);
  cr_assert(filterx_object_set_subscript(filterx_otel_array, NULL, &element_1_value));
  cr_assert(filterx_object_set_subscript(filterx_otel_array, NULL, &element_2_value));
  cr_assert(filterx_object_set_subscript(filterx_otel_array, NULL, &element_3_value));
  cr_assert(filterx_object_set_subscript(filterx_otel_array, NULL, &element_4_value));

  FilterXObject *invalid_element_key = filterx_string_new("invalid_key", -1);
  cr_assert_not(filterx_object_set_subscript(filterx_otel_array, invalid_element_key, &element_1_value));

  GString *serialized = g_string_new(NULL);
  LogMessageValueType type;
  cr_assert(filterx_object_marshal(filterx_otel_array, serialized, &type));

  cr_assert_eq(type, LM_VT_PROTOBUF);

  opentelemetry::proto::common::v1::ArrayValue array;
  cr_assert(array.ParsePartialFromArray(serialized->str, serialized->len));
  cr_assert_eq(array.values_size(), 4);
  cr_assert_eq(array.values(0).int_value(), 42);
  cr_assert_eq(array.values(1).string_value().compare("foobar"), 0);
  cr_assert_eq(array.values(2).value_case(), AnyValue::kKvlistValue);
  cr_assert(MessageDifferencer::Equals(array.values(2).kvlist_value(), KeyValueList()));
  cr_assert_eq(array.values(3).value_case(), AnyValue::kArrayValue);
  cr_assert(MessageDifferencer::Equals(array.values(3).array_value(), ArrayValue()));

  g_string_free(serialized, TRUE);
  filterx_object_unref(element_1_value);
  filterx_object_unref(element_2_value);
  filterx_object_unref(element_3_value);
  filterx_object_unref(element_4_value);
  filterx_object_unref(invalid_element_key);
  filterx_object_unref(filterx_otel_array);
}

Test(otel_filterx, array_through_logrecord)
{
  FilterXObject *fx_logrecord = filterx_otel_logrecord_new_from_args(NULL);
  FilterXObject *fx_array = filterx_otel_array_new_from_args(NULL);

  FilterXObject *fx_2 = filterx_integer_new(2);

  FilterXObject *fx_foo = filterx_string_new("foo", -1);
  FilterXObject *fx_bar = filterx_string_new("bar", -1);
  FilterXObject *fx_baz = filterx_string_new("baz", -1);

  FilterXObject *fx_inner_array = filterx_otel_array_new_from_args(NULL);

  /* objects for storing the result of getattr() and get_subscript() */
  FilterXObject *fx_get_1 = nullptr;
  FilterXObject *fx_get_2 = nullptr;

  /* $array[] = "foo"; */
  cr_assert(filterx_object_set_subscript(fx_array, nullptr, &fx_foo));

  /* $log.body = $array; */
  FilterXObject *fx_array_clone = filterx_object_clone(fx_array);
  cr_assert(filterx_object_setattr_string(fx_logrecord, "body", &fx_array_clone));
  filterx_object_unref(fx_array_clone);

  /* $log.body[] = "bar"; */
  fx_get_1 = filterx_object_getattr_string(fx_logrecord, "body");
  cr_assert(fx_get_1);
  cr_assert(filterx_object_set_subscript(fx_get_1, nullptr, &fx_bar));

  /* $array[] = "baz"; */
  cr_assert(filterx_object_set_subscript(fx_array, nullptr, &fx_baz));

  /* $log.body[] = $inner_array; */
  filterx_object_unref(fx_get_1);
  fx_get_1 = filterx_object_getattr_string(fx_logrecord, "body");
  cr_assert(fx_get_1);
  FilterXObject *fx_inner_array_clone = filterx_object_clone(fx_inner_array);
  cr_assert(filterx_object_set_subscript(fx_get_1, nullptr, &fx_inner_array_clone));
  filterx_object_unref(fx_inner_array_clone);

  /* $inner_array[] = "foo"; */
  cr_assert(filterx_object_set_subscript(fx_inner_array, nullptr, &fx_foo));

  /* $log.body[2][] = "baz"; */
  filterx_object_unref(fx_get_1);
  fx_get_1 = filterx_object_getattr_string(fx_logrecord, "body");
  cr_assert(fx_get_1);
  filterx_object_unref(fx_get_2);
  fx_get_2 = filterx_object_get_subscript(fx_get_1, fx_2);
  cr_assert(fx_get_2);
  cr_assert(filterx_object_set_subscript(fx_get_2, nullptr, &fx_baz));

  LogRecord expected_logrecord;
  ArrayValue *expected_logrecord_array = expected_logrecord.mutable_body()->mutable_array_value();
  expected_logrecord_array->add_values()->set_string_value("foo");
  expected_logrecord_array->add_values()->set_string_value("bar");
  ArrayValue *inner_array = expected_logrecord_array->add_values()->mutable_array_value();
  inner_array->add_values()->set_string_value("baz");
  cr_assert(MessageDifferencer::Equals(expected_logrecord, ((FilterXOtelLogRecord *) fx_logrecord)->cpp->get_value()));

  ArrayValue expected_array;
  expected_array.add_values()->set_string_value("foo");
  expected_array.add_values()->set_string_value("baz");
  cr_assert(MessageDifferencer::Equals(expected_array, ((FilterXOtelArray *) fx_array)->cpp->get_value()));

  ArrayValue expected_inner_array;
  expected_inner_array.add_values()->set_string_value("foo");
  cr_assert(MessageDifferencer::Equals(expected_inner_array, ((FilterXOtelArray *) fx_inner_array)->cpp->get_value()));

  filterx_object_unref(fx_get_2);
  filterx_object_unref(fx_get_1);
  filterx_object_unref(fx_inner_array);
  filterx_object_unref(fx_baz);
  filterx_object_unref(fx_bar);
  filterx_object_unref(fx_foo);
  filterx_object_unref(fx_2);
  filterx_object_unref(fx_array);
  filterx_object_unref(fx_logrecord);
}

void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
  otel_filterx_objects_global_init();
}

void
teardown(void)
{
  cfg_free(configuration);
  app_shutdown();
}

TestSuite(otel_filterx, .init = setup, .fini = teardown);
