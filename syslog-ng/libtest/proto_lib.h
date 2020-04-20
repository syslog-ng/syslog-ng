/*
 * Copyright (c) 2012-2019 Balabit
 * Copyright (c) 2012 Balázs Scheidler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef PROTO_LIB_H_INCLUDED
#define PROTO_LIB_H_INCLUDED

#include "testutils.h"
#include "logproto/logproto-server.h"

extern LogProtoServerOptions proto_server_options;

void assert_proto_server_status(LogProtoServer *proto, LogProtoStatus status, LogProtoStatus expected_status);
void assert_proto_server_fetch(LogProtoServer *proto, const gchar *expected_msg, gssize expected_msg_len);
void assert_proto_server_fetch_single_read(LogProtoServer *proto, const gchar *expected_msg, gssize expected_msg_len);
void assert_proto_server_fetch_failure(LogProtoServer *proto, LogProtoStatus expected_status,
                                       const gchar *error_message);
void assert_proto_server_fetch_ignored_eof(LogProtoServer *proto);

LogProtoServer *construct_server_proto_plugin(const gchar *name, LogTransport *transport);

void init_proto_tests(void);
void deinit_proto_tests(void);

static inline LogProtoServerOptions *
get_inited_proto_server_options(void)
{
  log_proto_server_options_init(&proto_server_options, configuration);
  return &proto_server_options;
}

#endif
