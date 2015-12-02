/*
 * Copyright (c) 2012-2013 Balabit
 * Copyright (c) 2012-2013 Balázs Scheidler
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

#ifndef TEST_LOGPROTO_H_INCLUDED
#define TEST_LOGPROTO_H_INCLUDED

void test_log_proto_server_options(void);
void test_log_proto_record_server(void);
void test_log_proto_text_server(void);
void test_log_proto_indented_multiline_server(void);
void test_log_proto_regexp_multiline_server(void);
void test_log_proto_dgram_server(void);
void test_log_proto_framed_server(void);

#endif
