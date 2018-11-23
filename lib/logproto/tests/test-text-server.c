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

#include "mock-transport.h"
#include "proto_lib.h"
#include "msg_parse_lib.h"
#include "logproto/logproto-text-server.h"

#include <errno.h>

/****************************************************************************************
 * LogProtoTextServer
 ****************************************************************************************/

LogProtoServer *
construct_test_proto(LogTransport *transport)
{
  proto_server_options.max_msg_size = 32;

  return log_proto_text_server_new(transport, get_inited_proto_server_options());
}

static void
test_log_proto_text_server_no_encoding(gboolean input_is_stream)
{
  LogProtoServer *proto;

  proto = construct_test_proto(
            /* 32 bytes max line length */
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)(
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

static void
test_log_proto_text_server_no_eol_before_eof(void)
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

static void
test_log_proto_text_server_eol_before_eof(void)
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

static void
test_log_proto_text_server_io_error_before_eof(void)
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

static void
test_log_proto_text_server_partial_chars_before_eof(void)
{
  LogProtoServer *proto;

  log_proto_server_options_set_encoding(&proto_server_options, "utf-8");
  proto = construct_test_proto(
            log_transport_mock_stream_new(
              /* utf8 */
              "\xc3", -1,
              LTM_EOF));

  assert_true(log_proto_server_validate_options(proto),
              "validate_options() returned failure but it should have succeeded");
  assert_proto_server_fetch_failure(proto, LPS_EOF,
                                    "EOF read on a channel with leftovers from previous character conversion, dropping input");
  log_proto_server_free(proto);
}

static void
test_log_proto_text_server_not_fixed_encoding(void)
{
  LogProtoServer *proto;

  log_proto_server_options_set_encoding(&proto_server_options, "utf-8");

  /* to test whether a non-easily-reversable charset works too */
  proto = construct_test_proto(
            log_transport_mock_stream_new(
              /* utf8 */
              "árvíztűrőtükörfúrógép\n", -1,
              LTM_EOF));
  assert_true(log_proto_server_validate_options(proto),
              "validate_options() returned failure but it should have succeeded");
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);
}

static void
test_log_proto_text_server_ucs4(void)
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

  assert_true(log_proto_server_validate_options(proto),
              "validate_options() returned failure but it should have succeeded");
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);
}

static void
test_log_proto_text_server_iso8859_2(void)
{
  LogProtoServer *proto;

  log_proto_server_options_set_encoding(&proto_server_options, "iso-8859-2");
  proto = construct_test_proto(
            log_transport_mock_stream_new(
              /* iso-8859-2 */
              "\xe1\x72\x76\xed\x7a\x74\xfb\x72\xf5\x74\xfc\x6b\xf6\x72\x66\xfa"      /*  |árvíztűrőtükörfú| */
              "\x72\xf3\x67\xe9\x70\n", -1,                                           /*  |rógép|            */
              LTM_EOF));

  assert_true(log_proto_server_validate_options(proto),
              "validate_options() returned failure but it should have succeeded");
  assert_proto_server_fetch(proto, "árvíztűrőtükörfúrógép", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);
  log_proto_server_free(proto);
}

static void
test_log_proto_text_server_invalid_encoding(void)
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
  assert_grabbed_messages_contain("Unknown character set name specified; encoding='never-ever-is-going-to-be-such-an-encoding'",
                                  "message about unknown charset missing");
  assert_false(success, "validate_options() returned success but it should have failed");
  log_proto_server_free(proto);
}

static void
test_log_proto_text_server_multi_read(void)
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

static void
test_log_proto_text_server_multi_read_not_allowed(void)
{
  /* FIXME: */
#if 0
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
#endif
}

LogProtoServer *
construct_test_proto_with_accumulator(gint (*accumulator)(LogProtoTextServer *, const guchar *, gsize, gssize),
                                      LogTransport *transport)
{
  LogProtoServer *proto = construct_test_proto(transport);

  ((LogProtoTextServer *) proto)->accumulate_line = accumulator;
  return proto;
}

static gint accumulate_seq;

static gint
accumulator_delay_lines(LogProtoTextServer *self, const guchar *msg, gsize msg_len, gssize consumed_len)
{
  accumulate_seq++;
  if ((accumulate_seq % 2) == 0)
    return LPT_REWIND_LINE | LPT_EXTRACTED;
  else
    return LPT_CONSUME_LINE | LPT_WAITING;
}

static void
test_log_proto_text_server_is_not_fetching_input_as_long_as_there_is_an_eol_in_buffer(void)
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
accumulator_assert_that_lines_are_starting_with_sequence_number(LogProtoTextServer *self, const guchar *msg,
    gsize msg_len, gssize consumed_len)
{
  assert_true((msg[0] - '0') == accumulate_seq,
              "accumulate_line: Message doesn't start with sequence number, msg=%.*s, seq=%d",
              (int)msg_len, msg, accumulate_seq);
  assert_gint(consumed_len, -1, "Initial invocation of the accumulator expects -1 as consumed_len");
  accumulate_seq++;
  return LPT_CONSUME_LINE | LPT_EXTRACTED;
}

static void
test_log_proto_text_server_accumulate_line_is_called_for_each_line(gboolean input_is_stream)
{
  LogProtoServer *proto;

  accumulate_seq = 0;
  proto = construct_test_proto_with_accumulator(
            accumulator_assert_that_lines_are_starting_with_sequence_number,
            /* 32 bytes max line length */
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)(
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

static gint
accumulator_extract_pairs(LogProtoTextServer *self, const guchar *msg, gsize msg_len, gssize consumed_len)
{
  accumulate_seq++;
  if ((accumulate_seq % 2) == 0)
    return LPT_CONSUME_LINE | LPT_EXTRACTED;
  else
    return LPT_CONSUME_LINE | LPT_WAITING;
}

static void
test_log_proto_text_server_accumulate_line_can_consume_lines_without_returning_them(gboolean input_is_stream)
{
  LogProtoServer *proto;

  accumulate_seq = 0;
  proto = construct_test_proto_with_accumulator(
            accumulator_extract_pairs,
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)(
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

static gint
accumulator_join_continuation_lines(LogProtoTextServer *self, const guchar *msg, gsize msg_len, gssize consumed_len)
{
  if (consumed_len >= 0 && msg_len > consumed_len + 1)
    {
      gchar first_char = msg[consumed_len + 1];

      if (first_char == ' ')
        return LPT_CONSUME_LINE | LPT_WAITING;
      else
        return LPT_REWIND_LINE | LPT_EXTRACTED;
    }
  else
    {
      return LPT_CONSUME_LINE | LPT_WAITING;
    }
}

static void
test_log_proto_text_server_accumulate_line_can_rewind_lines_if_uninteresting(gboolean input_is_stream)
{
  LogProtoServer *proto;

  accumulate_seq = 0;
  proto = construct_test_proto_with_accumulator(
            accumulator_join_continuation_lines,
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)(
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

static void
test_log_proto_text_server_accumulation_terminated_if_input_is_closed(gboolean input_is_stream)
{
  LogProtoServer *proto;

  accumulate_seq = 0;
  proto = construct_test_proto_with_accumulator(
            accumulator_join_continuation_lines,
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)(
              "0 line\n"
              " line\n"
              "1 line\n", -1,
              LTM_EOF));

  assert_proto_server_fetch(proto, "0 line\n line", -1);
  assert_proto_server_fetch(proto, "1 line", -1);

  log_proto_server_free(proto);
}

static void
test_log_proto_text_server_accumulation_terminated_if_buffer_full(gboolean input_is_stream)
{
  LogProtoServer *proto;

  accumulate_seq = 0;
  proto = construct_test_proto_with_accumulator(
            accumulator_join_continuation_lines,
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)(
              "0123456789abcdef\n"
              " 0123456789abcdef\n"
              " continuation\n", -1,
              LTM_EOF));

  assert_proto_server_fetch(proto, "0123456789abcdef\n 0123456789abcd", -1);
  assert_proto_server_fetch(proto, "ef\n continuation", -1);
  log_proto_server_free(proto);
}

static gint
accumulator_rewind_initial(LogProtoTextServer *self, const guchar *msg, gsize msg_len, gssize consumed_len)
{
  accumulate_seq++;
  if (accumulate_seq == 1)
    return LPT_REWIND_LINE | LPT_EXTRACTED;
  return LPT_CONSUME_LINE | LPT_EXTRACTED;
}

static void
test_log_proto_text_server_rewinding_the_initial_line_results_in_an_empty_message(gboolean input_is_stream)
{
  LogProtoServer *proto;

  accumulate_seq = 0;
  proto = construct_test_proto_with_accumulator(
            accumulator_rewind_initial,
            (input_is_stream ? log_transport_mock_stream_new : log_transport_mock_records_new)(
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

void
test_log_proto_text_server(void)
{
  PROTO_TESTCASE(test_log_proto_text_server_no_encoding, FALSE);
  PROTO_TESTCASE(test_log_proto_text_server_no_encoding, TRUE);
  PROTO_TESTCASE(test_log_proto_text_server_no_eol_before_eof);
  PROTO_TESTCASE(test_log_proto_text_server_eol_before_eof);
  PROTO_TESTCASE(test_log_proto_text_server_io_error_before_eof);
  PROTO_TESTCASE(test_log_proto_text_server_partial_chars_before_eof);
  PROTO_TESTCASE(test_log_proto_text_server_not_fixed_encoding);
  PROTO_TESTCASE(test_log_proto_text_server_ucs4);
  PROTO_TESTCASE(test_log_proto_text_server_iso8859_2);
  PROTO_TESTCASE(test_log_proto_text_server_invalid_encoding);
  PROTO_TESTCASE(test_log_proto_text_server_multi_read);
  PROTO_TESTCASE(test_log_proto_text_server_multi_read_not_allowed);
  PROTO_TESTCASE(test_log_proto_text_server_is_not_fetching_input_as_long_as_there_is_an_eol_in_buffer);
  PROTO_TESTCASE(test_log_proto_text_server_accumulation_terminated_if_input_is_closed, FALSE);
  PROTO_TESTCASE(test_log_proto_text_server_accumulation_terminated_if_input_is_closed, TRUE);
  PROTO_TESTCASE(test_log_proto_text_server_accumulation_terminated_if_buffer_full, TRUE);
  PROTO_TESTCASE(test_log_proto_text_server_accumulation_terminated_if_buffer_full, FALSE);
  PROTO_TESTCASE(test_log_proto_text_server_rewinding_the_initial_line_results_in_an_empty_message, FALSE);
  PROTO_TESTCASE(test_log_proto_text_server_rewinding_the_initial_line_results_in_an_empty_message, TRUE);
  PROTO_TESTCASE(test_log_proto_text_server_accumulate_line_is_called_for_each_line, FALSE);
  PROTO_TESTCASE(test_log_proto_text_server_accumulate_line_is_called_for_each_line, TRUE);
  PROTO_TESTCASE(test_log_proto_text_server_accumulate_line_can_consume_lines_without_returning_them, TRUE);
  PROTO_TESTCASE(test_log_proto_text_server_accumulate_line_can_consume_lines_without_returning_them, FALSE);
  PROTO_TESTCASE(test_log_proto_text_server_accumulate_line_can_rewind_lines_if_uninteresting, TRUE);
  PROTO_TESTCASE(test_log_proto_text_server_accumulate_line_can_rewind_lines_if_uninteresting, FALSE);
}
