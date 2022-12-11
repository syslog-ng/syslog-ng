/*
 * Copyright (c) 2013-2019 Balabit
 * Copyright (c) 2013-2014 Balázs Scheidler <balazs.scheidler@balabit.com>
 * Copyright (c) 2014 Viktor Tusa <viktor.tusa@balabit.com>
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

#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include "libtest/mock-transport.h"
#include "libtest/proto_lib.h"
#include "libtest/msg_parse_lib.h"

#include "multi-line/multi-line-factory.h"
#include "logproto/logproto-multiline-server.h"


static LogProtoServer *
log_proto_prefix_garbage_multiline_server_new(LogTransport *transport,
                                              const gchar *prefix,
                                              const gchar *garbage)
{
  MultiLineOptions multi_line_options;

  multi_line_options_defaults(&multi_line_options);
  multi_line_options_set_mode(&multi_line_options, "prefix-garbage");
  if (prefix)
    multi_line_options_set_prefix(&multi_line_options, prefix, NULL);
  if (garbage)
    multi_line_options_set_garbage(&multi_line_options, garbage, NULL);

  LogProtoServer *server = log_proto_multiline_server_new(transport, get_inited_proto_server_options(),
                                                          multi_line_factory_construct(&multi_line_options));
  multi_line_options_destroy(&multi_line_options);
  return server;
}

static LogProtoServer *
log_proto_prefix_suffix_multiline_server_new(LogTransport *transport,
                                             const gchar *prefix,
                                             const gchar *suffix)
{
  MultiLineOptions multi_line_options;

  multi_line_options_defaults(&multi_line_options);
  multi_line_options_set_mode(&multi_line_options, "prefix-suffix");
  if (prefix)
    multi_line_options_set_prefix(&multi_line_options, prefix, NULL);
  if (suffix)
    multi_line_options_set_garbage(&multi_line_options, suffix, NULL);

  LogProtoServer *server = log_proto_multiline_server_new(transport, get_inited_proto_server_options(),
                                                          multi_line_factory_construct(&multi_line_options));
  multi_line_options_destroy(&multi_line_options);
  return server;
}


ParameterizedTestParameters(log_proto, test_lines_separated_with_prefix)
{
  static LogTransportMockConstructor log_transport_mock_new_data_list[] =
  {
    log_transport_mock_stream_new,
    log_transport_mock_records_new,
  };

  return cr_make_param_array(
           LogTransportMockConstructor,
           log_transport_mock_new_data_list,
           G_N_ELEMENTS(log_transport_mock_new_data_list));
}

ParameterizedTest(LogTransportMockConstructor *log_transport_mock_new, log_proto, test_lines_separated_with_prefix)
{
  LogProtoServer *proto;

  proto = log_proto_prefix_garbage_multiline_server_new(
            /* 32 bytes max line length, which means that the complete
             * multi-line block plus one additional line must fit into 32
             * bytes. */
            (*log_transport_mock_new)(
              "Foo First Line\n"
              "Foo Second Line\n"
              "Foo Third Line\n"
              "Foo Multiline\n"
              "multi\n"
              "Foo final\n", -1,
              LTM_PADDING,
              LTM_EOF),
            "^Foo", NULL);

  assert_proto_server_fetch(proto, "Foo First Line", -1);
  assert_proto_server_fetch(proto, "Foo Second Line", -1);
  assert_proto_server_fetch(proto, "Foo Third Line", -1);
  assert_proto_server_fetch(proto, "Foo Multiline\nmulti", -1);

  log_proto_server_free(proto);
}

ParameterizedTestParameters(log_proto, test_lines_separated_with_prefix_and_garbage)
{
  static LogTransportMockConstructor log_transport_mock_new_data_list[] =
  {
    log_transport_mock_stream_new,
    log_transport_mock_records_new,
  };

  return cr_make_param_array(
           LogTransportMockConstructor,
           log_transport_mock_new_data_list,
           G_N_ELEMENTS(log_transport_mock_new_data_list));
}

ParameterizedTest(LogTransportMockConstructor *log_transport_mock_new, log_proto,
                  test_lines_separated_with_prefix_and_garbage)
{
  LogProtoServer *proto;

  proto = log_proto_prefix_garbage_multiline_server_new(
            /* 32 bytes max line length, which means that the complete
             * multi-line block plus one additional line must fit into 32
             * bytes. */
            (*log_transport_mock_new)(
              "Foo First Line Bar\n"
              "Foo Second Line Bar\n"
              "Foo Third Line Bar\n"
              "Foo Multiline\n"
              "multi Bar\n"
              "Foo final\n", -1,
              LTM_PADDING,
              LTM_EOF),
            "^Foo", " Bar$");

  assert_proto_server_fetch(proto, "Foo First Line", -1);
  assert_proto_server_fetch(proto, "Foo Second Line", -1);
  assert_proto_server_fetch(proto, "Foo Third Line", -1);
  assert_proto_server_fetch(proto, "Foo Multiline\nmulti", -1);

  log_proto_server_free(proto);
}

ParameterizedTestParameters(log_proto, test_lines_separated_with_prefix_and_suffix)
{
  static LogTransportMockConstructor log_transport_mock_new_data_list[] =
  {
    log_transport_mock_stream_new,
    log_transport_mock_records_new,
  };

  return cr_make_param_array(
           LogTransportMockConstructor,
           log_transport_mock_new_data_list,
           G_N_ELEMENTS(log_transport_mock_new_data_list));
}

ParameterizedTest(LogTransportMockConstructor *log_transport_mock_new, log_proto,
                  test_lines_separated_with_prefix_and_suffix)
{
  LogProtoServer *proto;

  proto = log_proto_prefix_suffix_multiline_server_new(
            /* 32 bytes max line length, which means that the complete
             * multi-line block plus one additional line must fit into 32
             * bytes. */
            (*log_transport_mock_new)(
              "prefix first suffix garbage\n"
              "prefix multi\n"
              "suffix garbage\n"
              "prefix final\n", -1,
              LTM_PADDING,
              LTM_EOF),
            "^prefix", "suffix");

  assert_proto_server_fetch(proto, "prefix first suffix", -1);
  assert_proto_server_fetch(proto, "prefix multi\nsuffix", -1);

  log_proto_server_free(proto);
}

ParameterizedTestParameters(log_proto, test_lines_separated_with_garbage)
{
  static LogTransportMockConstructor log_transport_mock_new_data_list[] =
  {
    log_transport_mock_stream_new,
    log_transport_mock_records_new,
  };

  return cr_make_param_array(
           LogTransportMockConstructor,
           log_transport_mock_new_data_list,
           G_N_ELEMENTS(log_transport_mock_new_data_list));
}

ParameterizedTest(LogTransportMockConstructor *log_transport_mock_new, log_proto, test_lines_separated_with_garbage)
{
  LogProtoServer *proto;

  proto = log_proto_prefix_garbage_multiline_server_new(
            /* 32 bytes max line length, which means that the complete
             * multi-line block plus one additional line must fit into 32
             * bytes. */
            (*log_transport_mock_new)(
              "Foo First Line Bar\n"
              "Foo Second Line Bar\n"
              "Foo Third Line Bar\n"
              "Foo Multiline\n"
              "multi Bar\n"
              "Foo final\n", -1,
              LTM_PADDING,
              LTM_EOF),
            NULL, " Bar$");

  assert_proto_server_fetch(proto, "Foo First Line", -1);
  assert_proto_server_fetch(proto, "Foo Second Line", -1);
  assert_proto_server_fetch(proto, "Foo Third Line", -1);
  assert_proto_server_fetch(proto, "Foo Multiline\nmulti", -1);

  log_proto_server_free(proto);
}

ParameterizedTestParameters(log_proto, test_first_line_without_prefix)
{
  static LogTransportMockConstructor log_transport_mock_new_data_list[] =
  {
    log_transport_mock_stream_new,
    log_transport_mock_records_new,
  };

  return cr_make_param_array(
           LogTransportMockConstructor,
           log_transport_mock_new_data_list,
           G_N_ELEMENTS(log_transport_mock_new_data_list));
}

ParameterizedTest(LogTransportMockConstructor *log_transport_mock_new, log_proto, test_first_line_without_prefix)
{
  LogProtoServer *proto;

  proto = log_proto_prefix_garbage_multiline_server_new(
            /* 32 bytes max line length, which means that the complete
             * multi-line block plus one additional line must fit into 32
             * bytes. */
            (*log_transport_mock_new)(
              "First Line\n"
              "Foo Second Line\n"
              "Foo Third Line\n"
              "Foo Multiline\n"
              "multi\n"
              "Foo final\n", -1,
              LTM_PADDING,
              LTM_EOF),
            "^Foo", NULL);

  assert_proto_server_fetch(proto, "First Line", -1);
  assert_proto_server_fetch(proto, "Foo Second Line", -1);
  assert_proto_server_fetch(proto, "Foo Third Line", -1);
  assert_proto_server_fetch(proto, "Foo Multiline\nmulti", -1);

  log_proto_server_free(proto);
}
