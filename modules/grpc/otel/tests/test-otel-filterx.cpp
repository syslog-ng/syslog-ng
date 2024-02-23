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

#include "filterx/otel-logrecord.hpp"
#include "filterx/otel-resource.hpp"
#include "filterx/otel-scope.hpp"

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


/* LogRecord */

Test(otel_filterx, logrecord_empty)
{
  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) otel_logrecord(NULL);
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

  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) otel_logrecord(args);
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

  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) otel_logrecord(args);
  cr_assert_not(filterx_otel_logrecord);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, logrecord_from_protobuf_malformed_data)
{
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new("1234", 4));

  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) otel_logrecord(args);
  cr_assert_not(filterx_otel_logrecord);

  g_ptr_array_free(args, TRUE);
}

Test(otel_filterx, logrecord_too_many_args)
{
  GPtrArray *args = g_ptr_array_new_full(2, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_string_new("foo", 3));
  g_ptr_array_insert(args, 1, filterx_protobuf_new("bar", 3));

  FilterXOtelLogRecord *filterx_otel_logrecord = (FilterXOtelLogRecord *) otel_logrecord(args);
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

  std::string serialized_resource = resource.SerializePartialAsString();
  GPtrArray *args = g_ptr_array_new_full(1, (GDestroyNotify) filterx_object_unref);
  g_ptr_array_insert(args, 0, filterx_protobuf_new(serialized_resource.c_str(), serialized_resource.length()));

  FilterXObject *filterx_otel_resource = (FilterXObject *) otel_resource_new(args);
  cr_assert(filterx_otel_resource);

  _assert_filterx_integer_attribute(filterx_otel_resource, "dropped_attributes_count", 42);

  // TODO: check the "attributes" repeated KeyValue when we implement support for that.

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

  // TODO: check the "attributes" repeated KeyValue when we implement support for that.

  cr_assert_not(filterx_object_setattr(filterx_otel_resource, "invalid_attr", filterx_integer));

  GString *serialized = g_string_new(NULL);
  LogMessageValueType type;
  cr_assert(filterx_object_marshal(filterx_otel_resource, serialized, &type));

  cr_assert_eq(type, LM_VT_PROTOBUF);

  opentelemetry::proto::resource::v1::Resource resource;
  cr_assert(resource.ParsePartialFromArray(serialized->str, serialized->len));
  cr_assert_eq(resource.dropped_attributes_count(), 42);

  g_string_free(serialized, TRUE);
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
