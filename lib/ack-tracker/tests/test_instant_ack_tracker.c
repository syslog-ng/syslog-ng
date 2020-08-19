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
#include "ack-tracker/instant_ack_tracker.h"
#include "ack-tracker/ack_tracker_factory.h"
#include "logsource.h"
#include "apphook.h"

GlobalConfig *cfg;

typedef struct _TestBookmarkData
{
  guint *saved_ctr;
} TestBookmarkData;

static void
_save_bookmark(Bookmark *bookmark)
{
  TestBookmarkData *bookmark_data = (TestBookmarkData *) &bookmark->container;
  (*bookmark_data->saved_ctr)++;
}

static void
_fill_bookmark(Bookmark *bookmark, guint *ctr)
{
  TestBookmarkData *bookmark_data = (TestBookmarkData *) &bookmark->container;

  bookmark_data->saved_ctr = ctr;
  bookmark->save = _save_bookmark;
}

typedef struct _TestLogPipeDst
{
  LogPipe super;
} TestLogPipeDst;

static gboolean
_test_logpipe_dst_init(LogPipe *s)
{
  return TRUE;
}

static void
_test_logpipe_dst_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
}
static TestLogPipeDst *
_init_test_logpipe_dst(void)
{
  TestLogPipeDst *dst = g_new0(TestLogPipeDst, 1);

  log_pipe_init_instance(&dst->super, cfg);
  dst->super.init = _test_logpipe_dst_init;
  dst->super.queue = _test_logpipe_dst_queue;

  cr_assert(log_pipe_init(&dst->super));

  return dst;
}

static void
_deinit_test_logpipe_dst(TestLogPipeDst *dst)
{
  log_pipe_deinit(&dst->super);
  log_pipe_unref(&dst->super);
}

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

TestSuite(instant_ack_tracker_bookmarkless, .init = _setup, .fini = _teardown);

Test(instant_ack_tracker_bookmarkless, request_bookmark_returns_the_same_bookmark)
{
  LogSource *src = _init_log_source(instant_ack_tracker_bookmarkless_factory_new());
  cr_assert_not_null(src->ack_tracker);
  AckTracker *ack_tracker = src->ack_tracker;
  Bookmark *bm1 = ack_tracker_request_bookmark(ack_tracker);
  Bookmark *bm2 = ack_tracker_request_bookmark(ack_tracker);
  cr_expect_eq(bm1, bm2);
  _deinit_log_source(src);
}

Test(instant_ack_tracker_bookmarkless, bookmark_save_not_called_when_acked)
{
  LogSource *src = _init_log_source(instant_ack_tracker_bookmarkless_factory_new());
  TestLogPipeDst *dst = _init_test_logpipe_dst();
  log_pipe_append(&src->super, &dst->super);
  cr_assert_not_null(src->ack_tracker);
  AckTracker *ack_tracker = src->ack_tracker;
  Bookmark *bm = ack_tracker_request_bookmark(ack_tracker);
  guint saved_ctr = 0;
  _fill_bookmark(bm, &saved_ctr);
  LogMessage *msg = log_msg_new_empty();
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 10);
  log_source_post(src, msg);
  // this is why we need a dummy 'destination' : keep back ACK
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 9);
  cr_assert_eq(msg->ack_record->tracker, ack_tracker);
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  log_msg_ack(msg, &path_options, AT_PROCESSED);
  // okay, ACK is done, but save counter is 0 -> so Bookmark::save not called
  cr_expect_eq(saved_ctr, 0);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 10);
  log_msg_unref(msg);
  _deinit_log_source(src);
  _deinit_test_logpipe_dst(dst);
}

Test(instant_ack_tracker_bookmarkless, same_bookmark_for_all_messages)
{
  LogSource *src = _init_log_source(instant_ack_tracker_bookmarkless_factory_new());
  cr_assert_not_null(src->ack_tracker);
  AckTracker *ack_tracker = src->ack_tracker;
  Bookmark *bm1 = ack_tracker_request_bookmark(ack_tracker);
  LogMessage *msg1 = log_msg_new_empty();
  ack_tracker_track_msg(ack_tracker, msg1);
  Bookmark *bm2 = ack_tracker_request_bookmark(ack_tracker);
  LogMessage *msg2 = log_msg_new_empty();
  ack_tracker_track_msg(ack_tracker, msg2);
  ack_tracker_manage_msg_ack(ack_tracker, msg1, AT_PROCESSED);
  ack_tracker_manage_msg_ack(ack_tracker, msg2, AT_PROCESSED);
  cr_expect_eq(bm1, bm2);
  _deinit_log_source(src);
}


TestSuite(instant_ack_tracker, .init = _setup, .fini = _teardown);

Test(instant_ack_tracker, request_bookmark_returns_same_bookmarks_until_pending_not_assigned)
{
  LogSource *src = _init_log_source(instant_ack_tracker_factory_new());
  cr_assert_not_null(src->ack_tracker);
  AckTracker *ack_tracker = src->ack_tracker;
  Bookmark *bm1 = ack_tracker_request_bookmark(ack_tracker);
  Bookmark *bm2 = ack_tracker_request_bookmark(ack_tracker);
  cr_expect_eq(bm1, bm2);
  LogMessage *msg = log_msg_new_empty();
  ack_tracker_track_msg(ack_tracker, msg);
  log_msg_ref(msg);
  bm2 = ack_tracker_request_bookmark(ack_tracker);
  cr_expect_neq(bm1, bm2);
  ack_tracker_manage_msg_ack(ack_tracker, msg, AT_PROCESSED);
  log_msg_unref(msg);
  _deinit_log_source(src);
}

Test(instant_ack_tracker, bookmark_saving)
{
  LogSource *src = _init_log_source(instant_ack_tracker_factory_new());
  TestLogPipeDst *dst = _init_test_logpipe_dst();
  log_pipe_append(&src->super, &dst->super);
  cr_assert_not_null(src->ack_tracker);
  AckTracker *ack_tracker = src->ack_tracker;
  Bookmark *bm = ack_tracker_request_bookmark(ack_tracker);
  guint saved_ctr = 0;
  _fill_bookmark(bm, &saved_ctr);
  LogMessage *msg = log_msg_new_empty();
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 10);
  log_source_post(src, msg);
  // this is why we need a dummy 'destination' : keep back ACK
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 9);
  cr_assert_eq(msg->ack_record->tracker, ack_tracker);
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  log_msg_ack(msg, &path_options, AT_PROCESSED);
  // okay, ACK is done, save counter is 1
  cr_expect_eq(saved_ctr, 1);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 10);
  log_msg_unref(msg);
  _deinit_log_source(src);
  _deinit_test_logpipe_dst(dst);
}
