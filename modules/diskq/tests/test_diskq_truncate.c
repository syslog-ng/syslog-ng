/*
 * Copyright (c) 2018 Balabit
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

#include "logqueue.h"
#include "logqueue-disk.h"
#include "logqueue-disk-reliable.h"
#include "logqueue-disk-non-reliable.h"
#include "diskq.h"
#include "qdisk.c"
#include "apphook.h"

#include "queue_utils_lib.h"
#include "test_diskq_tools.h"
#include "testutils.h"

#include <stdlib.h>
#include <sys/stat.h>


static LogQueue *
_get_diskqueue(gchar *filename, DiskQueueOptions *options)
{
  LogQueue *q = log_queue_disk_non_reliable_new(options, NULL);
  log_queue_set_use_backlog(q, FALSE);
  log_queue_disk_load_queue(q, filename);
  return q;
}

static LogQueue *
_create_diskqueue(gchar *filename, DiskQueueOptions *options)
{
  LogQueue *q;
  unlink(filename);

  _construct_options(options, 1100000, 0, FALSE);
  options->qout_size = 40;
  q = _get_diskqueue(filename, options);
  return q;
}

static void
_save_diskqueue(LogQueue *q)
{
  gboolean persistent;
  log_queue_disk_save_queue(q, &persistent);
  log_queue_unref(q);
}

static gsize
_calculate_expected_file_size(gint number_of_msgs)
{
  guint diskq_record_len_size = 4;
  gsize one_msg_size = get_one_message_serialized_size();
  return ((one_msg_size + diskq_record_len_size) * number_of_msgs) + QDISK_RESERVED_SPACE;
}

static void
_assert_diskq_actual_file_size_with_stored(LogQueue *q)
{
  QDisk *qdisk = ((LogQueueDisk *)q)->qdisk;
  gchar *filename = qdisk->filename;

  struct stat diskq_file_stat;
  assert_gint(stat(filename, &diskq_file_stat), 0, "Stat call failed, errno:%d", errno);
  assert_gint(qdisk->file_size, diskq_file_stat.st_size, "File size does not match with stored size");
  fprintf(stderr, "Actual file size: %ld\n", diskq_file_stat.st_size);
}

static void
_assert_diskq_actual_file_size_with_expected(LogQueue *q, gboolean should_file_be_empty,
                                             gint64 number_of_messages_on_disk)
{
  QDisk *qdisk = ((LogQueueDisk *)q)->qdisk;
  gchar *filename = qdisk->filename;

  struct stat diskq_file_stat;
  assert_gint(stat(filename, &diskq_file_stat), 0, "Stat call failed, errno:%d", errno);

  if (should_file_be_empty)
    {
      assert_gint(diskq_file_stat.st_size, QDISK_RESERVED_SPACE, "Truncate after load failed: file is not empty");
    }
  else
    {
      // diskq file is truncated on read when it becomes empty,
      // thus we have to assert to the original file size (after push finished), i.e. the max recorded size
      assert_gint(diskq_file_stat.st_size, _calculate_expected_file_size(number_of_messages_on_disk),
                  "Truncate after load failed: expected size differs");
    }
}

typedef struct
{
  const gchar *test_id;
  gint number_of_msgs_to_push;
  gint number_of_msgs_to_pop;
  gboolean should_file_be_empty_after_truncate;
} TruncateTestParams;

static void
_test_diskq_truncate(TruncateTestParams params)
{
  LogQueue *q;
  DiskQueueOptions options;
  GString *filename = g_string_new("test_dq_truncate.qf");
  fprintf(stderr, "### Test case starts: %s\n", params.test_id);

  q = _create_diskqueue(filename->str, &options);
  assert_gint(log_queue_get_length(q), 0, "No messages should be in a newly created disk-queue file!");

  feed_some_messages(q, params.number_of_msgs_to_push);
  assert_gint(log_queue_get_length(q), params.number_of_msgs_to_push, "Not all messages have been queued!");
  gint64 messages_on_disk_after_push = qdisk_get_length(((LogQueueDisk *)q)->qdisk);
  _assert_diskq_actual_file_size_with_stored(q);

  send_some_messages(q, params.number_of_msgs_to_pop);
  assert_gint(log_queue_get_length(q), params.number_of_msgs_to_push - params.number_of_msgs_to_pop,
              "Invalid number of messages in disk-queue after messages have been popped!");

  _save_diskqueue(q);

  q = _get_diskqueue(filename->str, &options);
  assert_gint(log_queue_get_length(q), params.number_of_msgs_to_push - params.number_of_msgs_to_pop,
              "Invalid number of messages in disk-queue after opened existing one");
  _assert_diskq_actual_file_size_with_stored(q);
  _assert_diskq_actual_file_size_with_expected(q, params.should_file_be_empty_after_truncate,
                                               messages_on_disk_after_push);


  unlink(filename->str);
  g_string_free(filename, TRUE);

  log_queue_unref(q);
  disk_queue_options_destroy(&options);
}

static void
test_diskq_truncate_with_diskbuffer_used(void)
{
  _test_diskq_truncate((TruncateTestParams)
  {
    .test_id = __func__,
     .number_of_msgs_to_push = 100,
      .number_of_msgs_to_pop = 50,
       .should_file_be_empty_after_truncate = FALSE
  });
}

static void
test_diskq_truncate_without_diskbuffer_used(void)
{
  _test_diskq_truncate((TruncateTestParams)
  {
    .test_id = __func__,
     .number_of_msgs_to_push = 100,
      .number_of_msgs_to_pop = 80,
       .should_file_be_empty_after_truncate = TRUE
  });
}


int
main(void)
{
  msg_init(TRUE); // internal messages will go to stderr(), msg_init() is idempotent
  app_startup();

  configuration = cfg_new_snippet();
  assert_true(cfg_init(configuration), "cfg_init failed!");

  // Diskbuffer is the part of disk-queue that is used only when qout is full
  test_diskq_truncate_with_diskbuffer_used();
  test_diskq_truncate_without_diskbuffer_used();

  app_shutdown();
  cfg_free(configuration);

  return 0;
}
