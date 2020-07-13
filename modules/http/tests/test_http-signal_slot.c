/*
 * Copyright (c) 2020 Balabit
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

#include "syslog-ng.h"
#include <apphook.h>
#include "http.h"
#include "http-worker.h"
#include "http-signals.h"
#include <criterion/criterion.h>

MainLoop *main_loop;
MsgFormatOptions parse_options;
MainLoopOptions main_loop_options;

HTTPDestinationDriver *driver;

static void
setup(void)
{
  debug_flag = TRUE;
  log_stderr = TRUE;
  app_startup();

  main_loop = main_loop_get_instance();
  main_loop_init(main_loop, &main_loop_options);

  driver = (HTTPDestinationDriver *) http_dd_new(main_loop_get_current_config(main_loop));
}

static void
teardown(void)
{
  main_loop_sync_worker_startup_and_teardown();
  log_pipe_deinit((LogPipe *)driver);
  log_pipe_unref((LogPipe *)driver);

  main_loop_deinit(main_loop);
  app_shutdown();
}

TestSuite(test_http_signal_slot, .init = setup, .fini = teardown);

static void
_generate_message(HTTPDestinationDriver *dd, const gchar *msg_str)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT_NOACK;
  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "MSG", msg_str, -1);
  log_pipe_queue(&dd->super.super.super.super, msg, &path_options);
}

gboolean signal_slot_executed;

static void
_sleep_msec(long msec)
{
  struct timespec sleep_time = { msec / 1000, (msec % 1000) * 1000000 };
  nanosleep(&sleep_time, NULL);
}

static void
wait_for_signal_slot_to_finish(void)
{
  const int MAX_SPIN_ITERATIONS = 10000;
  int c = 0;
  while (!signal_slot_executed && c++ < MAX_SPIN_ITERATIONS)
    _sleep_msec(1);
  cr_assert(signal_slot_executed, "signal slot did not execute");
}

static void
notify_signal_slot_finish(void)
{
  signal_slot_executed = TRUE;
}

static void
_check(const gchar *expected_body, HttpHeaderRequestSignalData *data)
{
  cr_assert_str_eq(data->request_body->str, expected_body);

  notify_signal_slot_finish();
}

Test(test_http_signal_slot, basic)
{
  SignalSlotConnector *ssc = driver->super.super.super.super.signal_slot_connector;

  const gchar *test_msg = "test message";
  CONNECT(ssc, signal_http_header_request, _check, test_msg);

  cr_assert(log_pipe_init((LogPipe *)driver));
  cr_assert(log_pipe_on_config_inited((LogPipe *)driver));

  _generate_message(driver, test_msg);

  wait_for_signal_slot_to_finish();
}

Test(test_http_signal_slot, single_with_prefix_suffix)
{
  http_dd_set_body_prefix((LogDriver *)driver, "[");
  http_dd_set_body_suffix((LogDriver *)driver, "]");
  http_dd_set_delimiter((LogDriver *)driver, ",");

  SignalSlotConnector *ssc = driver->super.super.super.super.signal_slot_connector;

  CONNECT(ssc, signal_http_header_request, _check, "[almafa]");

  cr_assert(log_pipe_init((LogPipe *)driver));
  cr_assert(log_pipe_on_config_inited((LogPipe *)driver));

  _generate_message(driver, "almafa");

  wait_for_signal_slot_to_finish();
}

Test(test_http_signal_slot, batch_with_prefix_suffix)
{
  http_dd_set_body_prefix((LogDriver *)driver, "[");
  http_dd_set_body_suffix((LogDriver *)driver, "]");
  http_dd_set_delimiter((LogDriver *)driver, ",");
  log_threaded_dest_driver_set_batch_lines((LogDriver *)driver, 2);
  log_threaded_dest_driver_set_batch_timeout((LogDriver *)driver, 1000);

  SignalSlotConnector *ssc = driver->super.super.super.super.signal_slot_connector;

  CONNECT(ssc, signal_http_header_request, _check, "[1,2]");

  cr_assert(log_pipe_init((LogPipe *)driver));
  cr_assert(log_pipe_on_config_inited((LogPipe *)driver));

  _generate_message(driver, "1");
  _generate_message(driver, "2");

  wait_for_signal_slot_to_finish();
}
