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


#include "otel-protobuf-formatter.hpp"
#include "otel-protobuf-parser.hpp"

#include "compat/cpp-start.h"
#include "apphook.h"
#include "cfg.h"
#include "compat/cpp-end.h"

#include <criterion/criterion.h>

using namespace syslogng::grpc::otel;

using namespace opentelemetry::proto::resource::v1;
using namespace opentelemetry::proto::common::v1;

static LogMessage *
_create_log_msg_with_dummy_resource_and_scope()
{
  LogMessage *msg = log_msg_new_empty();

  log_msg_set_value_by_name_with_type(msg, ".otel.resource.attributes.attr_0", "val_0", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.resource.dropped_attributes_count", "1", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.resource.schema_url", "resource_schema_url", -1, LM_VT_STRING);

  log_msg_set_value_by_name_with_type(msg, ".otel.scope.name", "scope", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.scope.version", "v1.2.3", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.scope.attributes.attr_1", "val_1", -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, ".otel.scope.dropped_attributes_count", "2", -1, LM_VT_INTEGER);
  log_msg_set_value_by_name_with_type(msg, ".otel.scope.schema_url", "scope_schema_url", -1, LM_VT_STRING);

  return msg;
}

static void
_assert_dummy_resource_and_scope(const Resource &resource, const std::string &resource_schema_url,
                                 const InstrumentationScope &scope, const std::string &scope_schema_url)
{
  cr_assert_eq(resource.attributes_size(), 1, "%d");
  cr_assert_str_eq(resource.attributes(0).key().c_str(), "attr_0");
  cr_assert_str_eq(resource.attributes(0).value().string_value().c_str(), "val_0");
  cr_assert_eq(resource.dropped_attributes_count(), 1);
  cr_assert_str_eq(resource_schema_url.c_str(), "resource_schema_url");

  cr_assert_str_eq(scope.name().c_str(), "scope");
  cr_assert_str_eq(scope.version().c_str(), "v1.2.3");
  cr_assert_eq(scope.attributes_size(), 1);
  cr_assert_str_eq(scope.attributes(0).key().c_str(), "attr_1");
  cr_assert_str_eq(scope.attributes(0).value().string_value().c_str(), "val_1");
  cr_assert_eq(scope.dropped_attributes_count(), 2);
  cr_assert_str_eq(scope_schema_url.c_str(), "scope_schema_url");
}

Test(otel_protobuf_formatter, get_message_type)
{
  LogMessage *msg = log_msg_new_empty();
  cr_assert_eq(get_message_type(msg), MessageType::UNKNOWN);

  log_msg_set_value_by_name_with_type(msg, ".otel_raw.type", "log", -1, LM_VT_BYTES);
  cr_assert_eq(get_message_type(msg), MessageType::UNKNOWN);

  log_msg_set_value_by_name_with_type(msg, ".otel_raw.type", "log", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::LOG);

  log_msg_set_value_by_name_with_type(msg, ".otel_raw.type", "metric", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::METRIC);

  log_msg_set_value_by_name_with_type(msg, ".otel_raw.type", "span", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::SPAN);

  log_msg_set_value_by_name_with_type(msg, ".otel_raw.type", "almafa", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::UNKNOWN);

  log_msg_unset_value_by_name(msg, ".otel_raw.type");

  log_msg_set_value_by_name_with_type(msg, ".otel.type", "log", -1, LM_VT_BYTES);
  cr_assert_eq(get_message_type(msg), MessageType::UNKNOWN);

  log_msg_set_value_by_name_with_type(msg, ".otel.type", "log", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::LOG);

  log_msg_set_value_by_name_with_type(msg, ".otel.type", "metric", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::METRIC);

  log_msg_set_value_by_name_with_type(msg, ".otel.type", "span", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::SPAN);

  log_msg_set_value_by_name_with_type(msg, ".otel.type", "almafa", -1, LM_VT_STRING);
  cr_assert_eq(get_message_type(msg), MessageType::UNKNOWN);

  log_msg_unref(msg);
}

Test(otel_protobuf_formatter, get_metadata)
{
  ProtobufFormatter formatter(configuration);
  LogMessage *msg = _create_log_msg_with_dummy_resource_and_scope();

  Resource resource;
  std::string resource_schema_url;
  InstrumentationScope scope;
  std::string scope_schema_url;
  cr_assert(formatter.get_metadata(msg, resource, resource_schema_url, scope, scope_schema_url));
  log_msg_unref(msg);

  _assert_dummy_resource_and_scope(resource, resource_schema_url, scope, scope_schema_url);

  /* Raw */
  msg = _create_log_msg_with_dummy_resource_and_scope();
  ProtobufParser::store_raw_metadata(msg, "", resource, resource_schema_url, scope, scope_schema_url);

  Resource resource_from_raw;
  std::string resource_schema_url_from_raw;
  InstrumentationScope scope_from_raw;
  std::string scope_schema_url_from_raw;
  cr_assert(formatter.get_metadata(msg, resource_from_raw, resource_schema_url_from_raw, scope_from_raw,
                                   scope_schema_url_from_raw));
  log_msg_unref(msg);

  _assert_dummy_resource_and_scope(resource_from_raw, resource_schema_url_from_raw, scope_from_raw,
                                   scope_schema_url_from_raw);
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

TestSuite(otel_protobuf_formatter, .init = setup, .fini = teardown);
