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
#include "timeutils/misc.h"
#include <iv.h>

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

typedef struct _TestBookmarkData
{
  guint *saved_ctr;
  guint *destroy_ctr;
} TestBookmarkData;

static void
_save_bookmark(Bookmark *bookmark)
{
  TestBookmarkData *bookmark_data = (TestBookmarkData *) &bookmark->container;
  (*bookmark_data->saved_ctr)++;
}

static void
_destroy_bookmark(Bookmark *bookmark)
{
  TestBookmarkData *bookmark_data = (TestBookmarkData *) &bookmark->container;
  (*bookmark_data->destroy_ctr)++;
}

static void
_fill_bookmark(Bookmark *bookmark, guint *saved_ctr, guint *destroy_ctr)
{
  TestBookmarkData *bookmark_data = (TestBookmarkData *) &bookmark->container;

  bookmark_data->saved_ctr = saved_ctr;
  bookmark_data->destroy_ctr = destroy_ctr;
  bookmark->save = _save_bookmark;
  bookmark->destroy = _destroy_bookmark;
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

static void
_dummy_on_batch_acked(GList *ack_records, gpointer user_data)
{
  gboolean *acked = (gboolean *) user_data;
  *acked = TRUE;
}

Test(batched_ack_tracker, request_bookmark_returns_the_same_until_not_track_msg)
{
  gboolean acked = FALSE;
  LogSource *src = _init_log_source(batched_ack_tracker_factory_new(0, 1, _dummy_on_batch_acked, &acked));
  cr_assert_not_null(src->ack_tracker);
  AckTracker *ack_tracker = src->ack_tracker;
  Bookmark *bm1 = ack_tracker_request_bookmark(ack_tracker);
  Bookmark *bm2 = ack_tracker_request_bookmark(ack_tracker);
  cr_expect_eq(bm1, bm2);
  LogMessage *msg = log_msg_new_empty();
  ack_tracker_track_msg(ack_tracker, msg);
  cr_expect_not(acked);
  Bookmark *bm3 = ack_tracker_request_bookmark(ack_tracker);
  cr_expect_neq(bm3, bm1);
  ack_tracker_manage_msg_ack(ack_tracker, msg, AT_PROCESSED);
  cr_expect(acked);
  _deinit_log_source(src);
}

static void
_ack(AckRecord *rec)
{
  bookmark_save(&rec->bookmark);
}

static void
_ack_all(GList *ack_records, gpointer user_data)
{
  if (user_data)
    {
      gboolean *cb_called = (gboolean *) user_data;
      *cb_called = TRUE;
    }
  g_list_foreach(ack_records, (GFunc) _ack, user_data);
}

Test(batched_ack_tracker, bookmark_saving)
{
  LogSource *src = _init_log_source(batched_ack_tracker_factory_new(0, 2, _ack_all, NULL));
  TestLogPipeDst *dst = _init_test_logpipe_dst();
  log_pipe_append(&src->super, &dst->super);
  cr_assert_not_null(src->ack_tracker);
  AckTracker *ack_tracker = src->ack_tracker;
  Bookmark *bm = ack_tracker_request_bookmark(ack_tracker);
  guint saved_ctr = 0;
  guint destroy_ctr = 0;
  _fill_bookmark(bm, &saved_ctr, &destroy_ctr);
  LogMessage *msg1 = log_msg_new_empty();
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 10);
  log_source_post(src, msg1);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 9);
  cr_assert_eq(msg1->ack_record->tracker, ack_tracker);
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  log_msg_ack(msg1, &path_options, AT_PROCESSED);
  cr_expect_eq(saved_ctr, 0);
  cr_expect_eq(destroy_ctr, 0);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 10);
  LogMessage *msg2 = log_msg_new_empty();
  bm = ack_tracker_request_bookmark(ack_tracker);
  _fill_bookmark(bm, &saved_ctr, &destroy_ctr);
  log_source_post(src, msg2);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 9);
  log_msg_ack(msg2, &path_options, AT_PROCESSED);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 10);
  cr_expect_eq(saved_ctr, 2);
  cr_expect_eq(destroy_ctr, 2);
  log_msg_unref(msg1);
  log_msg_unref(msg2);
  _deinit_log_source(src);
  _deinit_test_logpipe_dst(dst);
}

static void
_iv_quit(void *user_data)
{
  iv_quit();
}

static void
_run_iv_main_for_n_seconds(gint seconds)
{
  struct iv_timer wait_timer;
  IV_TIMER_INIT(&wait_timer);
  wait_timer.cookie = NULL;
  wait_timer.handler = _iv_quit;

  iv_validate_now();
  wait_timer.expires = iv_now;
  timespec_add_msec(&wait_timer.expires, seconds * 1000);

  iv_timer_register(&wait_timer);

  iv_main();
}

Test(batched_ack_tracker, batch_timeout)
{
  LogSource *src = _init_log_source(batched_ack_tracker_factory_new(500, 3, _ack_all, NULL));
  TestLogPipeDst *dst = _init_test_logpipe_dst();
  log_pipe_append(&src->super, &dst->super);
  cr_assert_not_null(src->ack_tracker);
  AckTracker *ack_tracker = src->ack_tracker;
  Bookmark *bm = ack_tracker_request_bookmark(ack_tracker);
  guint saved_ctr = 0;
  guint destroy_ctr = 0;
  _fill_bookmark(bm, &saved_ctr, &destroy_ctr);
  LogMessage *msg1 = log_msg_new_empty();
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 10);
  log_source_post(src, msg1);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 9);
  cr_assert_eq(msg1->ack_record->tracker, ack_tracker);
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  log_msg_ack(msg1, &path_options, AT_PROCESSED);
  cr_expect_eq(saved_ctr, 0);
  cr_expect_eq(destroy_ctr, 0);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 10);
  LogMessage *msg2 = log_msg_new_empty();
  bm = ack_tracker_request_bookmark(ack_tracker);
  _fill_bookmark(bm, &saved_ctr, &destroy_ctr);
  log_source_post(src, msg2);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 9);
  log_msg_ack(msg2, &path_options, AT_PROCESSED);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 10);

  _run_iv_main_for_n_seconds(1);

  cr_expect_eq(saved_ctr, 2);
  cr_expect_eq(destroy_ctr, 2);
  log_msg_unref(msg1);
  log_msg_unref(msg2);
  _deinit_log_source(src);
  _deinit_test_logpipe_dst(dst);
}

Test(batched_ack_tracker, deinit_acks_partial_batch)
{
  gboolean ack_cb_called = FALSE;
  LogSource *src = _init_log_source(batched_ack_tracker_factory_new(2000, 3, _ack_all, &ack_cb_called));
  TestLogPipeDst *dst = _init_test_logpipe_dst();
  log_pipe_append(&src->super, &dst->super);
  cr_assert_not_null(src->ack_tracker);
  AckTracker *ack_tracker = src->ack_tracker;
  Bookmark *bm = ack_tracker_request_bookmark(ack_tracker);
  guint saved_ctr = 0;
  guint destroy_ctr = 0;
  _fill_bookmark(bm, &saved_ctr, &destroy_ctr);
  LogMessage *msg1 = log_msg_new_empty();
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 10);
  log_source_post(src, msg1);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 9);
  cr_assert_eq(msg1->ack_record->tracker, ack_tracker);
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  log_msg_ack(msg1, &path_options, AT_PROCESSED);
  cr_expect_eq(saved_ctr, 0);
  cr_expect_eq(destroy_ctr, 0);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 10);
  LogMessage *msg2 = log_msg_new_empty();
  bm = ack_tracker_request_bookmark(ack_tracker);
  _fill_bookmark(bm, &saved_ctr, &destroy_ctr);
  log_source_post(src, msg2);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 9);
  log_msg_ack(msg2, &path_options, AT_PROCESSED);
  cr_expect_eq(window_size_counter_get(&src->window_size, NULL), 10);

  ack_tracker_deinit(ack_tracker);
  cr_expect(ack_cb_called);

  _run_iv_main_for_n_seconds(1);

  cr_expect_eq(saved_ctr, 2);
  cr_expect_eq(destroy_ctr, 2);
  log_msg_unref(msg1);
  log_msg_unref(msg2);
  _deinit_log_source(src);
  _deinit_test_logpipe_dst(dst);
}
