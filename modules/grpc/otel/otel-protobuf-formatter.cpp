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

syslogng::grpc::otel::MessageType
syslogng::grpc::otel::get_message_type(LogMessage *msg)
{
  gssize len;
  LogMessageValueType type;
  const gchar *value = log_msg_get_value_by_name_with_type(msg, ".otel_raw.type", &len, &type);

  if (type == LM_VT_NULL)
    value = log_msg_get_value_by_name_with_type(msg, ".otel.type", &len, &type);

  if (type != LM_VT_STRING)
    return MessageType::UNKNOWN;

  if (strncmp(value, "log", len) == 0)
    return MessageType::LOG;

  if (strncmp(value, "metric", len) == 0)
    return MessageType::METRIC;

  if (strncmp(value, "span", len) == 0)
    return MessageType::SPAN;

  return MessageType::UNKNOWN;
}
