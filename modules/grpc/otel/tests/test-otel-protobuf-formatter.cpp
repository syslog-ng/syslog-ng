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

#include "compat/cpp-start.h"
#include "apphook.h"
#include "compat/cpp-end.h"

#include <criterion/criterion.h>

using namespace syslogng::grpc::otel;

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

TestSuite(otel_protobuf_formatter, .init = app_startup, .fini = app_shutdown);
