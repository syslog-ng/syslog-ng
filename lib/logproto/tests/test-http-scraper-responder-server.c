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
#include <criterion/parameterized.h>
#include "libtest/mock-transport.h"
#include "libtest/proto_lib.h"
#include "libtest/msg_parse_lib.h"

#include "multi-line/empty-line-separated-multi-line.h"
#include "logproto/logproto-http-scraper-responder-server.h"

#include "stats/stats-prometheus.h"

#include <iv.h>

static LogProtoHTTPScraperResponderOptionsStorage *
get_inited_proto_http_scraper_server_options(void)
{
  log_proto_http_scraper_responder_options_defaults(&proto_server_options);
  log_proto_server_options_init(&proto_server_options, configuration);
  // This one is hackish a bit, and does not needed normally.
  // As we have to call log_proto_http_scraper_responder_options_destroy, bacause we have one common
  // proto_server_options instane now, the base options part will not call the
  // Without this test cases (like test_http_scraper_multiline) will not call the inherited `init` func
  // (because its initialized membe already set to TRUE)
  // G=Full teardown of the base options (log_proto_server_options_destroy) will not work either as both
  // log_proto_server_options_init and log_proto_server_options_destroy are called only once, at suit init/teardown
  log_proto_http_scraper_responder_options_init(&proto_server_options, configuration);
  return (LogProtoHTTPScraperResponderOptionsStorage *)&proto_server_options;
}

static inline void
proto_http_scraper_server_free(LogProtoServer *proto, LogProtoHTTPScraperResponderOptionsStorage *options)
{
  log_proto_http_scraper_responder_options_destroy((LogProtoServerOptionsStorage *)options);
  log_proto_server_free(proto);
}


static void
test_empty_crnl_multiline_at_eof_dont_keep_trailing_nl(LogTransportMockConstructor log_transport_mock_new)
{
  LogProtoHTTPScraperResponderOptionsStorage *options = get_inited_proto_http_scraper_server_options();
  // http-scraper uses EmpytLineSeparatedMultiLine that has keep_trailing_newline is set to TRUE by default
  multi_line_options_set_keep_trailing_newline(&options->super.super.super.multi_line_options, FALSE);
  LogProtoServer *proto = log_proto_http_scraper_responder_server_new(
                            log_transport_mock_new(
                              "GET /metrics HTTP/1.1\r\n", -1,
                              "Host: 192.168.1.111:8080\r\n", -1,
                              "\r\n", -1,
                              LTM_EOF),
                            (const LogProtoServerOptionsStorage *)options);

  assert_proto_server_fetch(proto, "GET /metrics HTTP/1.1\r\nHost: 192.168.1.111:8080", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);

  proto_http_scraper_server_free(proto, options);
}

static void
test_empty_nl_multiline_at_eof_dont_keep_trailing_nl(LogTransportMockConstructor log_transport_mock_new)
{
  LogProtoHTTPScraperResponderOptionsStorage *options = get_inited_proto_http_scraper_server_options();
  // http-scraper uses EmpytLineSeparatedMultiLine that has keep_trailing_newline is set to TRUE by default
  multi_line_options_set_keep_trailing_newline(&options->super.super.super.multi_line_options, FALSE);
  LogProtoServer *proto = log_proto_http_scraper_responder_server_new(
                            log_transport_mock_new(
                              "GET /metrics HTTP/1.1\n", -1,
                              "Host: 192.168.1.111:8080\n", -1,
                              "\n", -1,
                              LTM_EOF),
                            (const LogProtoServerOptionsStorage *)options);

  assert_proto_server_fetch(proto, "GET /metrics HTTP/1.1\nHost: 192.168.1.111:8080", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);

  proto_http_scraper_server_free(proto, options);
}

static void
test_empty_crnl_multiline_at_eof(LogTransportMockConstructor log_transport_mock_new)
{
  LogProtoHTTPScraperResponderOptionsStorage *options = get_inited_proto_http_scraper_server_options();
  LogProtoServer *proto = log_proto_http_scraper_responder_server_new(
                            log_transport_mock_new(
                              "GET /metrics HTTP/1.1\r\n", -1,
                              "Host: 192.168.1.111:8080\r\n", -1,
                              "\r\n", -1,
                              LTM_EOF),
                            (const LogProtoServerOptionsStorage *)options);

  // http-scraper uses EmpytLineSeparatedMultiLine that has keep_trailing_newline is set to TRUE by default
  assert_proto_server_fetch(proto, "GET /metrics HTTP/1.1\r\nHost: 192.168.1.111:8080\r\n\r\n", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);

  proto_http_scraper_server_free(proto, options);
}

// This should store the HTTP response header + the mocked stats_generate_prometheus/stats_execute_query_command results that cannot be more than 256 now
static gchar write_buff[256];
static const gchar *mocked_stats_prometheus_response = "syslogng_source_processed{id=\"s_prometheus_stat\"} 3";
static const gchar *mocked_query_prometheus_response = "syslogng_source_processed{id=\"s_prometheus_stat\"} 4";

// Mock of LogProtoHTTPScraperResponder::stats_generate_prometheus
void
stats_generate_prometheus(StatsPrometheusRecordFunc process_record, gpointer user_data, gboolean with_legacy,
                          gboolean *cancelled)
{
  gpointer *args = (gpointer *)user_data;
  GString *result = *(GString **)args[1];
  g_string_append(result, mocked_stats_prometheus_response);
}

// Mock of LogProtoHTTPScraperResponder::stats_execute_query_command
GString *
stats_execute_query_command(const gchar *command, gpointer user_data, gboolean *cancelled)
{
  GString *result = g_string_new(mocked_query_prometheus_response);
  return result;
}

static void
test_empty_nl_multiline_at_eof(LogTransportMockConstructor log_transport_mock_new)
{
  LogProtoHTTPScraperResponderOptionsStorage *options = get_inited_proto_http_scraper_server_options();
  LogProtoServer *proto = log_proto_http_scraper_responder_server_new(
                            log_transport_mock_new(
                              "GET /metrics HTTP/1.1\n", -1,
                              "Host: 192.168.1.111:8080\n", -1,
                              "\n", -1,
                              LTM_EOF),
                            (const LogProtoServerOptionsStorage *)options);

  // http-scraper uses EmpytLineSeparatedMultiLine that has keep_trailing_newline is set to TRUE by default
  assert_proto_server_fetch(proto, "GET /metrics HTTP/1.1\nHost: 192.168.1.111:8080\n\n", -1);
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);

  proto_http_scraper_server_free(proto, options);
}

Test(log_proto, test_http_scraper_multiline)
{
  test_empty_nl_multiline_at_eof(log_transport_mock_http_screaper_new);
  test_empty_crnl_multiline_at_eof(log_transport_mock_http_screaper_new);

  test_empty_nl_multiline_at_eof_dont_keep_trailing_nl(log_transport_mock_http_screaper_new);
  test_empty_crnl_multiline_at_eof_dont_keep_trailing_nl(log_transport_mock_http_screaper_new);
}

static void
test_scrape_limit(LogTransportMockConstructor log_transport_mock_new)
{
  LogProtoHTTPScraperResponderOptionsStorage *options = get_inited_proto_http_scraper_server_options();
  LogProtoServer *proto = log_proto_http_scraper_responder_server_new(
                            log_transport_mock_new(
                              "GET /metrics HTTP/1.1\n", -1,
                              "Host: 192.168.1.111:8080\n", -1,
                              "\n", -1,
                              "GET /metrics HTTP/1.1\n", -1,
                              "Host: 192.168.1.111:8080\n", -1,
                              "\n", -1,
                              "GET /metrics HTTP/1.1\n", -1,
                              "Host: 192.168.1.111:8080\n", -1,
                              "\n", -1,
                              "GET /metrics HTTP/1.1\n", -1,
                              "Host: 192.168.1.111:8080\n", -1,
                              "\n", -1,
                              LTM_EOF),
                            (const LogProtoServerOptionsStorage *)options);

  // http-scraper uses EmpytLineSeparatedMultiLine that has keep_trailing_newline is set to TRUE by default
  assert_proto_server_fetch(proto, "GET /metrics HTTP/1.1\nHost: 192.168.1.111:8080\n\n", -1);
  // http-scraper scrape-freq-limit() is set to 0 (no limit) by default, so immediate next fetch should succeed
  assert_proto_server_fetch(proto, "GET /metrics HTTP/1.1\nHost: 192.168.1.111:8080\n\n", -1);
  // setting the limit higher should lead an empty fetch result (and dropped fecth result)
  log_proto_http_scraper_responder_options_set_scrape_freq_limit((LogProtoServerOptionsStorage *)options, 1);
  iv_invalidate_now();
  assert_proto_server_fetch(proto, "", -1);
  // waiting till the limit passed and fetching again after ellapsed should succeed again
  sleep(2);
  iv_invalidate_now();
  assert_proto_server_fetch(proto, "GET /metrics HTTP/1.1\nHost: 192.168.1.111:8080\n\n", -1);
  // last fetch should be EOF (as result of the previous is dropped)
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);

  proto_http_scraper_server_free(proto, options);
}

Test(log_proto, test_http_scraper_freq_limit)
{
  test_scrape_limit(log_transport_mock_http_screaper_new);
}

static inline gssize
_log_transport_write(LogTransport *self, const gpointer buf, gsize count)
{
  strncpy(write_buff, buf, count);
  write_buff[count] = 0;
  return count;
}

static void
test_scrape_stat_and_query(LogTransportMockConstructor log_transport_mock_new, const gchar *expected_result,
                           const char *stat_type)
{
  LogProtoHTTPScraperResponderOptionsStorage *options = get_inited_proto_http_scraper_server_options();
  log_proto_http_scraper_responder_options_set_stat_type((LogProtoServerOptionsStorage *)options, stat_type);
  LogTransport *transport = log_transport_mock_new(
                              "GET /metrics HTTP/1.1\n", -1,
                              "Host: 192.168.1.111:8080\n", -1,
                              "\n", -1,
                              LTM_EOF);
  LogProtoServer *proto = log_proto_http_scraper_responder_server_new(transport,
                          (const LogProtoServerOptionsStorage *)options);

  // Mock the result writer
  transport->write = _log_transport_write;

  // http-scraper uses EmpytLineSeparatedMultiLine that has keep_trailing_newline is set to TRUE by default
  assert_proto_server_fetch(proto, "GET /metrics HTTP/1.1\nHost: 192.168.1.111:8080\n\n", -1);
  // last fetch should be EOF (as result of the previous is dropped)
  assert_proto_server_fetch_failure(proto, LPS_EOF, NULL);

  // the mocked write should store the whole HTTP response, we just want to compare the body part, the actual `stats` or `query` result (also mocked now)
  gint response_body_len = strlen(expected_result);
  gint write_buff_len = strlen(write_buff);
  gchar *result = &write_buff[write_buff_len - response_body_len];
  cr_assert_arr_eq((const gchar *) result, expected_result, response_body_len,
                   "LogProtoServer expected http-scraper response mismatch");

  proto_http_scraper_server_free(proto, options);
}

Test(log_proto, test_http_scraper_stat)
{
  test_scrape_stat_and_query(log_transport_mock_http_screaper_new, mocked_stats_prometheus_response, "stats");
}

Test(log_proto, test_http_scraper_query)
{
  test_scrape_stat_and_query(log_transport_mock_http_screaper_new, mocked_query_prometheus_response, "query");
}
