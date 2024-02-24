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

Test(syslog_ng_otlp, metadata)
{
  Resource resource;
  std::string resource_schema_url;
  InstrumentationScope scope;
  std::string scope_schema_url;

  ProtobufFormatter::get_metadata_for_syslog_ng(resource, resource_schema_url, scope, scope_schema_url);
  cr_assert(ProtobufParser::is_syslog_ng_log_record(resource, resource_schema_url, scope, scope_schema_url));
}

Test(syslog_ng_otlp, formatting_and_parsing)
{
  LogMessage *msg = log_msg_new_empty();

  UnixTime stamp = {.ut_sec = 1, .ut_usec = 2, .ut_gmtoff = 3};
  UnixTime recvd = {.ut_sec = 4, .ut_usec = 5, .ut_gmtoff = 6};
  guint16 pri = LOG_KERN | LOG_ERR;
  const char *tag = "foo_tag";
  const char *host = "foo_host";
  const char *host_from = "foo_host_from";
  const char *message = "foo_message";
  const char *program = "foo_program";
  const char *pid = "1234";
  const char *dot_nv_name = ".foo.name";
  const char *dot_nv_value = "42";
  LogMessageValueType dot_nv_type = LM_VT_INTEGER;
  const char *nv_name = "foo.name";
  const char *nv_value = "true";
  LogMessageValueType nv_type = LM_VT_BOOLEAN;
  const char *temp_nv_name = "0";
  const char *temp_nv_value = "foo_temp_value";
  LogMessageValueType temp_nv_type = LM_VT_STRING;

  msg->timestamps[LM_TS_STAMP] = stamp;
  msg->timestamps[LM_TS_RECVD] = recvd;
  msg->pri = pri;
  log_msg_set_tag_by_name(msg, tag);
  log_msg_set_value_by_name_with_type(msg, "HOST", host, -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, "HOST_FROM", host_from, -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, "MESSAGE", message, -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, "PROGRAM", program, -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, "PID", pid, -1, LM_VT_STRING);
  log_msg_set_value_by_name_with_type(msg, dot_nv_name, dot_nv_value, -1, dot_nv_type);
  log_msg_set_value_by_name_with_type(msg, nv_name, nv_value, -1, nv_type);
  log_msg_set_value_by_name_with_type(msg, temp_nv_name, temp_nv_value, -1, temp_nv_type);
  log_msg_set_value_by_name_with_type(msg, "SOURCE", "almafa", -1, LM_VT_STRING);

  LogRecord log_record;
  ProtobufFormatter(configuration).format_syslog_ng(msg, log_record);
  log_msg_unref(msg);

  msg = log_msg_new_empty();
  ProtobufParser::store_syslog_ng(msg, log_record);

  LogMessageValueType type;

  cr_assert_eq(memcmp(&msg->timestamps[LM_TS_STAMP], &stamp, sizeof(stamp)), 0);
  cr_assert_eq(memcmp(&msg->timestamps[LM_TS_RECVD], &recvd, sizeof(recvd)), 0);
  cr_assert_eq(msg->pri, pri);
  cr_assert_str_eq(log_msg_get_value_by_name_with_type(msg, "TAGS", NULL, &type), tag);
  cr_assert_eq(type, LM_VT_LIST);
  cr_assert_str_eq(log_msg_get_value_by_name_with_type(msg, "HOST", NULL, &type), host);
  cr_assert_eq(type, LM_VT_STRING);
  cr_assert_str_eq(log_msg_get_value_by_name_with_type(msg, "HOST_FROM", NULL, &type), host_from);
  cr_assert_eq(type, LM_VT_STRING);
  cr_assert_str_eq(log_msg_get_value_by_name_with_type(msg, "MESSAGE", NULL, &type), message);
  cr_assert_eq(type, LM_VT_STRING);
  cr_assert_str_eq(log_msg_get_value_by_name_with_type(msg, "PROGRAM", NULL, &type), program);
  cr_assert_eq(type, LM_VT_STRING);
  cr_assert_str_eq(log_msg_get_value_by_name_with_type(msg, "PID", NULL, &type), pid);
  cr_assert_eq(type, LM_VT_STRING);
  cr_assert_str_eq(log_msg_get_value_by_name_with_type(msg, dot_nv_name, NULL, &type), dot_nv_value);
  cr_assert_eq(type, dot_nv_type);
  cr_assert_str_eq(log_msg_get_value_by_name_with_type(msg, nv_name, NULL, &type), nv_value);
  cr_assert_eq(type, nv_type);
  /* Temp NVs (0, 1, ..., 256) are skipped. */
  cr_assert_str_eq(log_msg_get_value_by_name_with_type(msg, temp_nv_name, NULL, &type), "");
  cr_assert_eq(type, LM_VT_NULL);
  /* SOURCE should not be transferred .*/
  cr_assert_str_eq(log_msg_get_value_by_name_with_type(msg, "SOURCE", NULL, &type), "");
  cr_assert_eq(type, LM_VT_NULL);

  log_msg_unref(msg);
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

TestSuite(syslog_ng_otlp, .init = setup, .fini = teardown);
