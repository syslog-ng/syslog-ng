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

#include "compat/cpp-start.h"
#include "filterx/object-string.h"
#include "apphook.h"
#include "cfg.h"
#include "compat/cpp-end.h"

#include <google/protobuf/util/message_differencer.h>

#include <criterion/criterion.h>

using namespace syslogng::grpc::otel;
using namespace opentelemetry::proto::logs::v1;
using namespace opentelemetry::proto::common::v1;
using namespace google::protobuf::util;

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
