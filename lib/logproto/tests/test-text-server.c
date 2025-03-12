/*
 * Copyright (c) 2012-2019 Balabit
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

#include <criterion/criterion.h>
#include "libtest/mock-transport.h"
#include "libtest/proto_lib.h"
#include "libtest/msg_parse_lib.h"
#include "libtest/grab-logging.h"

#include "logproto/logproto-text-server.h"
#include "ack-tracker/ack_tracker_factory.h"

#include <errno.h>


static gint accumulate_seq;

LogProtoServer *
construct_test_proto(LogTransport *transport)
{
  proto_server_options.super.max_msg_size = 32;

  return log_proto_text_server_new(transport, get_inited_proto_server_options());
}

LogProtoServer *
construct_test_proto_with_nuls(LogTransport *transport)
{
  proto_server_options.super.max_msg_size = 32;

  return log_proto_text_with_nuls_server_new(transport, get_inited_proto_server_options());
}

LogProtoServer *
construct_test_proto_with_accumulator(gint (*accumulator)(MultiLineLogic *, const guchar *, gsize, const guchar *,
                                                          gsize),
                                      LogTransport *transport)
{
  MultiLineLogic *multi_line = g_new0(MultiLineLogic, 1);

  multi_line_logic_init_instance(multi_line);
  multi_line->accumulate_line = accumulator;

  LogProtoServer *proto = construct_test_proto(transport);

  ((LogProtoTextServer *) proto)->multi_line = multi_line;
  return proto;
}

static gint
accumulator_delay_lines(MultiLineLogic *self, const guchar *msg, gsize msg_len, const guchar *segment,
                        gsize segment_len)
{
  accumulate_seq++;
  if ((accumulate_seq % 2) == 0)
    return MLL_REWIND_SEGMENT | MLL_EXTRACTED;
  else
    return MLL_CONSUME_SEGMENT | MLL_WAITING;
}

static void
test_log_proto_text_server_no_encoding(LogTransportMockConstructor log_transport_mock_new)
{
  LogProtoServer *proto;

  proto = construct_test_proto(
            /* 32 bytes max line length */
            log_transport_mock_new(
              "01234567\n"
              /* line too long */
              "0123456789ABCDEF0123456789ABCDEF01234567\n"
              /* utf8 */
              "árvíztűrőtükörfúrógép\n"
              /* iso-8859-2 */
              "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"      /*  |árvíztűrőtükörfú| */
              "\x72\xf3\x67\xe9\x70\n",                                               /*  |rógép|            */
              -1,

              /* NUL terminated line */
              "01234567\0"
              "01234567\0\n"
              "01234567\n\0"
              "01234567\r\n\0", 40,

              "01234567\r\n"
              /* no eol before EOF */
              "01234567", -1,

              LTM_EOF));

  assert_proto_server_fetch(proto, "01234567", -1);

  /* input split due to an oversized input line */
  assert_proto_server_fetch(proto, "0123456789ABCDEF0123456789ABCDEF", -1);
  assert_proto_server_fetch(proto, "01234567", -1);

  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_server_fetch(proto,
                            "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"    /*  |árvíztűrőtükörfú| */
                            "\x72\xf3\x67\xe9\x70",                                               /*  |rógép|            */
                            -1);
  assert_proto_server_fetch(proto, "01234567", -1);

  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch(proto, "", -1);

  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch(proto, "", -1);

  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch(proto, "", -1);

  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch(proto, "01234567", -1);
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_no_encoding)
{
  test_log_proto_text_server_no_encoding(log_transport_mock_stream_new);
  test_log_proto_text_server_no_encoding(log_transport_mock_records_new);
}

Test(log_proto, test_log_proto_text_server_no_eol_before_eof)
{
  LogProtoServer *proto;

  proto = construct_test_proto(
            log_transport_mock_stream_new(
              /* no eol before EOF */
              "01234567", -1,

              LTM_EOF));

  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);

}

Test(log_proto, test_log_proto_text_with_embedded_nuls)
{
  LogProtoServer *proto;

  proto = construct_test_proto_with_nuls(
            log_transport_mock_stream_new(
              /* no eol before EOF */
              "01234567\n", -1,
              "alma\x00korte\n", 11,

              LTM_EOF));

  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch(proto, "alma\x00korte", 10);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_eol_before_eof)
{
  LogProtoServer *proto;

  proto = construct_test_proto(
            log_transport_mock_records_new(
              /* eol before EOF */
              "01234\n567\n890\n", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF));

  assert_proto_server_fetch(proto, "01234", -1);
  assert_proto_server_fetch(proto, "567", -1);
  assert_proto_server_fetch(proto, "890", -1);
  assert_proto_server_fetch_failure(proto, LPS_ERROR, NULL);
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_io_error_before_eof)
{
  LogProtoServer *proto;

  proto = construct_test_proto(
            log_transport_mock_stream_new(
              "01234567", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF));

  assert_proto_server_fetch(proto, "01234567", -1);
  assert_proto_server_fetch_failure(proto, LPS_ERROR, NULL);
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_partial_chars_before_eof)
{
  LogProtoServer *proto;

  log_proto_server_options_set_encoding(&proto_server_options, "utf-8");
  proto = construct_test_proto(
            log_transport_mock_stream_new(
              /* utf8 */
              "\xc3", -1,
              LTM_EOF));

  cr_assert(log_proto_server_validate_options(proto),
            "validate_options() returned failure but it should have succeeded");
  assert_proto_server_fetch_failure(proto, LPS_EOF,
                                    "EOF read on a channel with leftovers from previous character conversion, dropping input");
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_invalid_char_with_encoding)
{
  LogProtoServer *proto;

  log_proto_server_options_set_encoding(&proto_server_options, "GB18030");
  proto = construct_test_proto(
            log_transport_mock_stream_new(
              "foo" "\x80" "bar" "\x80" "baz", -1,
              LTM_EOF));

  cr_assert(log_proto_server_validate_options(proto),
            "validate_options() returned failure but it should have succeeded");
  assert_proto_server_fetch(proto, "foobarbaz", -1);
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_not_fixed_encoding)
{
  LogProtoServer *proto;

  log_proto_server_options_set_encoding(&proto_server_options, "utf-8");

  /* to test whether a non-easily-reversable charset works too */
  proto = construct_test_proto(
            log_transport_mock_stream_new(
              /* utf8 */
              "árvíztűrőtükörfúrógép\n", -1,
              LTM_EOF));
  cr_assert(log_proto_server_validate_options(proto),
            "validate_options() returned failure but it should have succeeded");
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_ucs4)
{
  LogProtoServer *proto;

  log_proto_server_options_set_encoding(&proto_server_options, "ucs-4");
  proto = construct_test_proto(
            log_transport_mock_stream_new(
              /* ucs4 */
              "\x00\x00\x00\xe1\x00\x00\x00\x72\x00\x00\x00\x76\x00\x00\x00\xed"      /* |...á...r...v...í| */
              "\x00\x00\x00\x7a\x00\x00\x00\x74\x00\x00\x01\x71\x00\x00\x00\x72"      /* |...z...t...ű...r| */
              "\x00\x00\x01\x51\x00\x00\x00\x74\x00\x00\x00\xfc\x00\x00\x00\x6b"      /* |...Q...t.......k| */
              "\x00\x00\x00\xf6\x00\x00\x00\x72\x00\x00\x00\x66\x00\x00\x00\xfa"      /* |.......r...f....| */
              "\x00\x00\x00\x72\x00\x00\x00\xf3\x00\x00\x00\x67\x00\x00\x00\xe9"      /* |...r.......g....| */
              "\x00\x00\x00\x70\x00\x00\x00\x0a", 88,                                 /* |...p....|         */
              LTM_EOF));

  cr_assert(log_proto_server_validate_options(proto),
            "validate_options() returned failure but it should have succeeded");
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_iso8859_2)
{
  LogProtoServer *proto;

  log_proto_server_options_set_encoding(&proto_server_options, "iso-8859-2");
  proto = construct_test_proto(
            log_transport_mock_stream_new(
              /* iso-8859-2 */
              "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"      /*  |árvíztűrőtükörfú| */
              "\x72\xf3\x67\xe9\x70\n", -1,                                           /*  |rógép|            */
              LTM_EOF));

  cr_assert(log_proto_server_validate_options(proto),
            "validate_options() returned failure but it should have succeeded");
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_invalid_encoding)
{
  LogProtoServer *proto;
  gboolean success;

  log_proto_server_options_set_encoding(&proto_server_options, "never-ever-is-going-to-be-such-an-encoding");
  proto = construct_test_proto(
            log_transport_mock_stream_new(
              "", -1,
              LTM_EOF));

  start_grabbing_messages();
  success = log_proto_server_validate_options(proto);
  assert_grabbed_log_contains("Unknown character set name specified; encoding='never-ever-is-going-to-be-such-an-encoding'");
  cr_assert_not(success, "validate_options() returned success but it should have failed");
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_multi_read)
{
  LogProtoServer *proto;

  proto = construct_test_proto(
            log_transport_mock_records_new(
              "foobar\n", -1,
              /* no EOL, proto implementation would read another chunk */
              "foobaz", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF));

  assert_proto_server_fetch(proto, "foobar", -1);
  assert_proto_server_fetch(proto, "foobaz", -1);
  assert_proto_server_fetch_failure(proto, LPS_ERROR, NULL);
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_multi_read_not_allowed, .disabled = true)
{
  /* FIXME: */
  LogProtoServer *proto;

  proto = construct_test_proto(
            log_transport_mock_records_new(
              "foobar\n", -1,
              /* no EOL, proto implementation would read another chunk */
              "foobaz", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF));

  ((LogProtoBufferedServer *) proto)->no_multi_read = TRUE;
  assert_proto_server_fetch_single_read(proto, "foobar", -1);
  /* because of EAGAIN */
  assert_proto_server_fetch_single_read(proto, NULL, -1);
  /* because of NOMREAD, partial lines are returned as empty */
  assert_proto_server_fetch_single_read(proto, NULL, -1);
  /* because of EAGAIN */
  assert_proto_server_fetch_single_read(proto, NULL, -1);
  /* error was detected by this time, partial line is returned before the error */
  assert_proto_server_fetch_single_read(proto, "foobaz", -1);
  /* finally the error is returned too */
  assert_proto_server_fetch_failure(proto, LPS_ERROR, NULL);
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_is_not_fetching_input_as_long_as_there_is_an_eol_in_buffer)
{
  LogProtoServer *proto;

  accumulate_seq = 0;
  proto = construct_test_proto_with_accumulator(
            accumulator_delay_lines,

            log_transport_mock_records_new(

              /* should the LogProto instance read ahead, it gets an
               * EIO error */
              "foo\n"
              "bar\n"
              "baz\n"
              "booz\n", -1,
              LTM_INJECT_ERROR(EIO),
              LTM_EOF));

  assert_proto_server_fetch(proto, "foo", -1);
  assert_proto_server_fetch(proto, "bar", -1);
  assert_proto_server_fetch(proto, "baz", -1);
  assert_proto_server_fetch(proto, "booz", -1);
  assert_proto_server_fetch_failure(proto, LPS_ERROR, NULL);
  log_proto_server_free(proto);
}

static gint
accumulator_assert_that_lines_are_starting_with_sequence_number(MultiLineLogic *self,
    const guchar *msg, gsize msg_len,
    const guchar *segment, gsize segment_len)
{
  cr_assert_eq((msg[0] - '0'), accumulate_seq,
               "accumulate_line: Message doesn't start with sequence number, msg=%.*s, seq=%d",
               (int)msg_len, msg, accumulate_seq);
  cr_assert_eq(msg_len, 0, "Initial invocation of the accumulator expects 0 as msg_len");
  accumulate_seq++;
  return MLL_CONSUME_SEGMENT | MLL_EXTRACTED;
}

static void
test_log_proto_text_server_accumulate_line_is_called_for_each_line(LogTransportMockConstructor log_transport_mock_new)
{
  LogProtoServer *proto;

  accumulate_seq = 0;
  proto = construct_test_proto_with_accumulator(
            accumulator_assert_that_lines_are_starting_with_sequence_number,
            /* 32 bytes max line length */
            log_transport_mock_new(
              "0 line\n"
              "1 line\n"
              "2 line\n"
              "3 line\n", -1,
              LTM_PADDING,
              LTM_EOF));

  assert_proto_server_fetch(proto, "0 line", -1);
  assert_proto_server_fetch(proto, "1 line", -1);
  assert_proto_server_fetch(proto, "2 line", -1);
  assert_proto_server_fetch(proto, "3 line", -1);

  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_accumulate_line_is_called_for_each_line)
{
  test_log_proto_text_server_accumulate_line_is_called_for_each_line(log_transport_mock_stream_new);
  test_log_proto_text_server_accumulate_line_is_called_for_each_line(log_transport_mock_records_new);
}

static gint
accumulator_extract_pairs(MultiLineLogic *self, const guchar *msg, gsize msg_len, const guchar *segment,
                          gsize segment_len)
{
  accumulate_seq++;
  if ((accumulate_seq % 2) == 0)
    return MLL_CONSUME_SEGMENT | MLL_EXTRACTED;
  else
    return MLL_CONSUME_SEGMENT | MLL_WAITING;
}

static void
test_log_proto_text_server_accumulate_line_can_consume_lines_without_returning_them(
  LogTransportMockConstructor log_transport_mock_new)
{
  LogProtoServer *proto;

  accumulate_seq = 0;
  proto = construct_test_proto_with_accumulator(
            accumulator_extract_pairs,
            log_transport_mock_new(
              "0 line\n"
              "1 line\n"
              "2 line\n"
              "3 line\n", -1,
              LTM_PADDING,
              LTM_EOF));

  assert_proto_server_fetch(proto, "0 line\n1 line", -1);
  assert_proto_server_fetch(proto, "2 line\n3 line", -1);

  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_accumulate_line_can_consume_lines_without_returning_them)
{
  test_log_proto_text_server_accumulate_line_can_consume_lines_without_returning_them(log_transport_mock_stream_new);
  test_log_proto_text_server_accumulate_line_can_consume_lines_without_returning_them(log_transport_mock_records_new);
}

static gint
accumulator_join_continuation_lines(MultiLineLogic *self, const guchar *msg, gsize msg_len, const guchar *segment,
                                    gsize segment_len)
{
  if (msg_len > 0 && segment_len > 0)
    {
      gchar first_char = segment[0];

      if (first_char == ' ')
        return MLL_CONSUME_SEGMENT | MLL_WAITING;
      else
        return MLL_REWIND_SEGMENT | MLL_EXTRACTED;
    }
  else
    {
      return MLL_CONSUME_SEGMENT | MLL_WAITING;
    }
}

static void
test_log_proto_text_server_accumulate_line_can_rewind_lines_if_uninteresting(LogTransportMockConstructor
    log_transport_mock_new)
{
  LogProtoServer *proto;

  accumulate_seq = 0;
  proto = construct_test_proto_with_accumulator(
            accumulator_join_continuation_lines,
            log_transport_mock_new(
              "0 line\n"
              " line\n"
              "2 line\n"
              " line\n"
              "3 end\n", -1,
              LTM_PADDING,
              LTM_EOF));

  assert_proto_server_fetch(proto, "0 line\n line", -1);
  assert_proto_server_fetch(proto, "2 line\n line", -1);

  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_accumulate_line_can_rewind_lines_if_uninteresting)
{
  test_log_proto_text_server_accumulate_line_can_rewind_lines_if_uninteresting(log_transport_mock_stream_new);
  test_log_proto_text_server_accumulate_line_can_rewind_lines_if_uninteresting(log_transport_mock_records_new);
}

static void
test_log_proto_text_server_accumulation_terminated_if_input_is_closed(LogTransportMockConstructor
    log_transport_mock_new)
{
  LogProtoServer *proto;

  accumulate_seq = 0;
  proto = construct_test_proto_with_accumulator(
            accumulator_join_continuation_lines,
            log_transport_mock_new(
              "0 line\n"
              " line\n"
              "1 line\n", -1,
              LTM_EOF));

  assert_proto_server_fetch(proto, "0 line\n line", -1);
  assert_proto_server_fetch(proto, "1 line", -1);

  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_accumulation_terminated_if_input_is_closed)
{
  test_log_proto_text_server_accumulation_terminated_if_input_is_closed(log_transport_mock_stream_new);
  test_log_proto_text_server_accumulation_terminated_if_input_is_closed(log_transport_mock_records_new);
}

static void
test_log_proto_text_server_accumulation_terminated_if_buffer_full(LogTransportMockConstructor log_transport_mock_new)
{
  LogProtoServer *proto;

  accumulate_seq = 0;
  proto = construct_test_proto_with_accumulator(
            accumulator_join_continuation_lines,
            log_transport_mock_new(
              "0123456789abcdef\n"
              " 0123456789abcdef\n"
              " continuation\n", -1,
              LTM_EOF));

  assert_proto_server_fetch(proto, "0123456789abcdef\n 0123456789abcd", -1);
  assert_proto_server_fetch(proto, "ef\n continuation", -1);
  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_accumulation_terminated_if_buffer_full)
{
  test_log_proto_text_server_accumulation_terminated_if_buffer_full(log_transport_mock_stream_new);
  test_log_proto_text_server_accumulation_terminated_if_buffer_full(log_transport_mock_records_new);
}

static gint
accumulator_rewind_initial(MultiLineLogic *self, const guchar *msg, gsize msg_len, const guchar *segment,
                           gsize segment_len)
{
  accumulate_seq++;
  if (accumulate_seq == 1)
    return MLL_REWIND_SEGMENT | MLL_EXTRACTED;
  return MLL_CONSUME_SEGMENT | MLL_EXTRACTED;
}

static void
test_log_proto_text_server_rewinding_the_initial_line_results_in_an_empty_message(LogTransportMockConstructor
    log_transport_mock_new)
{
  LogProtoServer *proto;

  accumulate_seq = 0;
  proto = construct_test_proto_with_accumulator(
            accumulator_rewind_initial,
            log_transport_mock_new(
              "0 line\n"
              "1 line\n"
              "2 line\n", -1,
              LTM_PADDING,
              LTM_EOF));

  assert_proto_server_fetch(proto, "", -1);
  assert_proto_server_fetch(proto, "0 line", -1);
  assert_proto_server_fetch(proto, "1 line", -1);
  assert_proto_server_fetch(proto, "2 line", -1);

  log_proto_server_free(proto);
}

Test(log_proto, test_log_proto_text_server_rewinding_the_initial_line_results_in_an_empty_message)
{
  test_log_proto_text_server_rewinding_the_initial_line_results_in_an_empty_message(log_transport_mock_stream_new);
  test_log_proto_text_server_rewinding_the_initial_line_results_in_an_empty_message(log_transport_mock_records_new);
}

Test(log_proto, test_log_proto_text_server_io_eagain)
{
  LogProtoServer *proto;

  proto = construct_test_proto(
            log_transport_mock_stream_new(
              "01234567\n", -1,
              LTM_INJECT_ERROR(EAGAIN),
              LTM_EOF));

  Bookmark bookmark;
  LogTransportAuxData aux;
  gboolean may_read = TRUE;
  const guchar *msg = NULL;
  gsize msg_len;

  log_transport_aux_data_init(&aux);
  cr_assert_eq(log_proto_server_fetch(proto, &msg, &msg_len, &may_read, &aux, &bookmark), LPS_SUCCESS);
  cr_assert_eq(log_proto_server_fetch(proto, &msg, &msg_len, &may_read, &aux, &bookmark), LPS_AGAIN);
  cr_assert_eq(log_proto_server_fetch(proto, &msg, &msg_len, &may_read, &aux, &bookmark), LPS_EOF);

  log_proto_server_free(proto);
}

Test(log_proto, buffer_split_with_encoding_and_position_tracking)
{
  GString *data = g_string_new("Lorem ipsum\xe2\x98\x83lor sit amet, consectetur adipiscing elit\n");
  GString *data_smaller = g_string_new("muspi merol\n");
  gsize bytes_should_not_fit = 5;

  proto_server_options.super.max_msg_size = data->len + data_smaller->len - bytes_should_not_fit;
  proto_server_options.super.max_buffer_size = proto_server_options.super.max_msg_size;
  log_proto_server_options_set_encoding(&proto_server_options, "utf-8");
  log_proto_server_options_set_ack_tracker_factory(&proto_server_options, consecutive_ack_tracker_factory_new());

  gchar *full_payload = g_strconcat(data_smaller->str, data->str, NULL);
  LogTransportMock *transport = (LogTransportMock *) log_transport_mock_records_new(full_payload, -1, LTM_EOF);

  LogProtoServer *proto = log_proto_text_server_new((LogTransport *) transport, get_inited_proto_server_options());

  start_grabbing_messages();
  assert_proto_server_fetch(proto, data_smaller->str, data_smaller->len - 1);
  assert_proto_server_fetch(proto, data->str, data->len - 1);
  stop_grabbing_messages();
  cr_assert_not(find_grabbed_message("Internal error"));

  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);

  log_proto_server_free(proto);
  g_free(full_payload);
  g_string_free(data_smaller, TRUE);
  g_string_free(data, TRUE);
}
