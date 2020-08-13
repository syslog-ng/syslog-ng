/*
 * Copyright (c) 2020 One Identity
 * Copyright (c) 2020 Laszlo Budai <laszlo.budai@outlook.com>
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
#include "ack-tracker/batched_ack_tracker.h"
#include "ack-tracker/ack_tracker_factory.h"
#include "logsource.h"
#include "apphook.h"

GlobalConfig *cfg;

static LogSource *
_init_log_source(AckTrackerFactory *factory)
{
  LogSource *src = g_new0(LogSource, 1);
  LogSourceOptions *options = g_new0(LogSourceOptions, 1);

  log_source_options_defaults(options);
  options->init_window_size = 10;
  log_source_init_instance(src, cfg);
  log_source_options_init(options, cfg, "testgroup");
  log_source_set_options(src, options, "test_stats_id", "test_stats_instance", TRUE, NULL);
  log_source_set_ack_tracker_factory(src, factory);

  cr_assert(log_pipe_init(&src->super));

  return src;
}

static void
_deinit_log_source(LogSource *src)
{
  log_pipe_deinit(&src->super);
  g_free(src->options);
  log_pipe_unref(&src->super);
}

static void
_setup(void)
{
  cfg = cfg_new_snippet();
  app_startup();
}

static void
_teardown(void)
{
  app_shutdown();
  cfg_free(cfg);
}

TestSuite(batched_ack_tracker, .init = _setup, .fini = _teardown);

Test(batched_ack_tracker, request_bookmark_returns_the_same_until_not_track_msg)
{
  LogSource *src = _init_log_source(batched_ack_tracker_factory_new(0, 0));
  cr_assert_not_null(src->ack_tracker);
  AckTracker *ack_tracker = src->ack_tracker;
  Bookmark *bm1 = ack_tracker_request_bookmark(ack_tracker);
  Bookmark *bm2 = ack_tracker_request_bookmark(ack_tracker);
  cr_expect_eq(bm1, bm2);
  LogMessage *msg = log_msg_new_empty();
  log_source_post(src, msg);
  Bookmark *bm3 = ack_tracker_request_bookmark(ack_tracker);
  cr_expect_neq(bm3, bm1);
  log_msg_unref(msg);
  _deinit_log_source(src);
}
