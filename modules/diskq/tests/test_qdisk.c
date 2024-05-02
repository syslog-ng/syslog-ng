/*
 * Copyright (c) 2022 One Identity
 * Copyright (c) 2022 László Várady
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

#include <criterion/criterion.h>

#include "syslog-ng.h"
#include "apphook.h"
#include "qdisk.h"
#include "scratch-buffers.h"

#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/* QDisk-internal: the frame is a 4-byte integer */
#define FRAME_LENGTH 4

#define DUMMY_RECORD_PATTERN ('z')
#define MiB(x) (x * 1024 * 1024)

typedef enum
{
  TDISKQ_NON_RELIABLE = 0,
  TDISKQ_RELIABLE = 1,
} TestDiskQType;

static DiskQueueOptions *
construct_diskq_options(TestDiskQType dq_type, gint64 capacity_bytes)
{
  DiskQueueOptions *opts = g_new0(DiskQueueOptions, 1);
  disk_queue_options_set_default_options(opts);
  disk_queue_options_capacity_bytes_set(opts, capacity_bytes);
  disk_queue_options_reliable_set(opts, dq_type);

  return opts;
}

static QDisk *
create_qdisk(TestDiskQType dq_type, const gchar *filename, gint64 capacity_bytes)
{
  DiskQueueOptions *opts = construct_diskq_options(dq_type, capacity_bytes);
  QDisk *qdisk = qdisk_new(opts, "TEST", filename);

  return qdisk;
}

static void
cleanup_qdisk(const gchar *filename, QDisk *qdisk)
{
  DiskQueueOptions *opts = qdisk_get_options(qdisk);

  qdisk_free(qdisk);
  disk_queue_options_destroy(opts);
  g_free(opts);

  unlink(filename);
}

gboolean
generate_dummy_payload(SerializeArchive *sa, gpointer user_data)
{
  guint size = GPOINTER_TO_UINT(user_data);

  ScratchBuffersMarker marker;
  GString *data = scratch_buffers_alloc_and_mark(&marker);
  for (guint i = 0; i < size; ++i)
    g_string_append_c(data, DUMMY_RECORD_PATTERN);

  serialize_archive_write_bytes(sa, data->str, size);

  scratch_buffers_reclaim_marked(marker);
  return TRUE;
}

gboolean
push_dummy_record(QDisk *qdisk, guint record_size)
{
  /* TODO: fix interface:
   * framing should be the responsibility of push_tail() because pop_head() expects framing
   */
  GString *data = g_string_new(NULL);
  GError *error;
  qdisk_serialize(data, generate_dummy_payload, GUINT_TO_POINTER(record_size), &error);
  gboolean success = qdisk_push_tail(qdisk, data);
  g_string_free(data, TRUE);

  return success;
}

void
assert_dummy_record(const GString *record, guint expected_size)
{
  cr_assert_eq(record->len, expected_size);
  for (guint i = 0; i < expected_size; ++i)
    {
      if (record->str[i] != DUMMY_RECORD_PATTERN)
        cr_assert(FALSE, "Invalid data was popped from QDisk, position: %u, actual: %c, expected: %c",
                  i, (guint) record->str[i], (guint) DUMMY_RECORD_PATTERN);
    }
}

gboolean
reliable_pop_record_without_backlog(QDisk *qdisk, GString *record)
{
  if (!qdisk_pop_head(qdisk, record))
    return FALSE;

  qdisk_empty_backlog(qdisk);
  return TRUE;
}

Test(qdisk, test_qdisk_started)
{
  const gchar *filename = "test_qdisk_started.rqf";
  QDisk *qdisk = create_qdisk(TDISKQ_RELIABLE, filename, MiB(1));

  cr_assert_not(qdisk_started(qdisk));

  qdisk_start(qdisk, NULL, NULL, NULL);
  cr_assert(qdisk_started(qdisk));

  qdisk_stop(qdisk, NULL, NULL, NULL);
  cr_assert_not(qdisk_started(qdisk));

  cleanup_qdisk(filename, qdisk);
}

Test(qdisk, qdisk_basic_push_pop)
{
  const gchar *filename = "test_qdisk_basic_push_pop.rqf";
  QDisk *qdisk = create_qdisk(TDISKQ_RELIABLE, filename, MiB(1));
  qdisk_start(qdisk, NULL, NULL, NULL);

  guint expected_record_len = 128;
  cr_assert(push_dummy_record(qdisk, expected_record_len));
  cr_assert_eq(qdisk_get_length(qdisk), 1);

  GString *popped_data = g_string_new(NULL);
  cr_assert(reliable_pop_record_without_backlog(qdisk, popped_data));
  assert_dummy_record(popped_data, expected_record_len);
  g_string_free(popped_data, TRUE);

  cr_assert_eq(qdisk_get_length(qdisk), 0);

  qdisk_stop(qdisk, NULL, NULL, NULL);
  cleanup_qdisk(filename, qdisk);
}

Test(qdisk, qdisk_is_space_avail)
{
  const gchar *filename = "test_qdisk_is_space_avail.rqf";
  gsize qdisk_size = MiB(1);
  GString *data = g_string_new(NULL);
  QDisk *qdisk = create_qdisk(TDISKQ_RELIABLE, filename, qdisk_size);
  qdisk_start(qdisk, NULL, NULL, NULL);

  gsize available_space = qdisk_size - QDISK_RESERVED_SPACE;
  cr_assert(qdisk_is_space_avail(qdisk, available_space));

  gsize record_len = 600 * 1024;
  push_dummy_record(qdisk, record_len);
  available_space -= record_len + FRAME_LENGTH;
  cr_assert(qdisk_is_space_avail(qdisk, available_space));

  /* fill diskq (overwrite a bit) */
  push_dummy_record(qdisk, record_len);
  available_space -= record_len;
  cr_assert_not(qdisk_is_space_avail(qdisk, 1));

  reliable_pop_record_without_backlog(qdisk, data);
  cr_assert_not(qdisk_is_space_avail(qdisk, record_len + FRAME_LENGTH + 1),
                "There should not be more free space than the previously popped message size");

  record_len -= 100;
  push_dummy_record(qdisk, record_len);
  /* 1 byte of empty space (between backlog and write head) is reserved */
  cr_assert(qdisk_is_space_avail(qdisk, 100 - 1));

  qdisk_stop(qdisk, NULL, NULL, NULL);
  g_string_free(data, TRUE);
  cleanup_qdisk(filename, qdisk);
}

Test(qdisk, qdisk_remove_head)
{
  const gchar *filename = "test_qdisk_remove_head.rqf";
  QDisk *qdisk = create_qdisk(TDISKQ_RELIABLE, filename, MiB(1));
  qdisk_start(qdisk, NULL, NULL, NULL);

  push_dummy_record(qdisk, 128);
  cr_assert(qdisk_remove_head(qdisk));

  cr_assert_not(qdisk_remove_head(qdisk));

  push_dummy_record(qdisk, 128);
  push_dummy_record(qdisk, 128);
  cr_assert(qdisk_remove_head(qdisk));
  cr_assert(qdisk_remove_head(qdisk));

  cr_assert_not(qdisk_remove_head(qdisk));

  qdisk_stop(qdisk, NULL, NULL, NULL);
  cleanup_qdisk(filename, qdisk);
}

Test(qdisk, qdisk_basic_ack_rewind)
{
  const gchar *filename = "test_qdisk_basic_ack_rewind.rqf";
  QDisk *qdisk = create_qdisk(TDISKQ_RELIABLE, filename, MiB(1));
  qdisk_start(qdisk, NULL, NULL, NULL);

  gsize num_of_records = 100;

  for (gsize i = 1; i <= num_of_records; ++i)
    push_dummy_record(qdisk, 128);

  cr_assert_eq(qdisk_get_backlog_count(qdisk), 0);

  for (gsize i = 1; i <= num_of_records; ++i)
    {
      qdisk_remove_head(qdisk);
      cr_assert_eq(qdisk_get_backlog_count(qdisk), i);
    }

  gsize to_rewind = 10;
  for (gsize i = 1; i <= num_of_records - to_rewind; ++i)
    {
      cr_assert(qdisk_ack_backlog(qdisk));
      cr_assert_eq(qdisk_get_backlog_count(qdisk), num_of_records - i);
    }

  cr_assert(qdisk_rewind_backlog(qdisk, 3));
  to_rewind -= 3;
  cr_assert_eq(qdisk_get_backlog_count(qdisk), to_rewind);

  cr_assert(qdisk_rewind_backlog(qdisk, to_rewind));
  cr_assert_eq(qdisk_get_backlog_count(qdisk), 0);
  cr_assert_eq(qdisk_get_backlog_head(qdisk), qdisk_get_reader_head(qdisk));

  qdisk_stop(qdisk, NULL, NULL, NULL);
  cleanup_qdisk(filename, qdisk);
}

Test(qdisk, qdisk_empty_backlog)
{
  const gchar *filename = "test_qdisk_empty_backlog.rqf";
  QDisk *qdisk = create_qdisk(TDISKQ_RELIABLE, filename, MiB(1));
  qdisk_start(qdisk, NULL, NULL, NULL);

  push_dummy_record(qdisk, 514);
  push_dummy_record(qdisk, 514);

  cr_assert_eq(qdisk_get_backlog_count(qdisk), 0);

  qdisk_remove_head(qdisk);
  qdisk_remove_head(qdisk);
  cr_assert_eq(qdisk_get_backlog_count(qdisk), 2);
  qdisk_empty_backlog(qdisk);
  cr_assert_eq(qdisk_get_backlog_count(qdisk), 0);

  cr_assert_eq(qdisk_get_backlog_head(qdisk), qdisk_get_reader_head(qdisk));

  qdisk_stop(qdisk, NULL, NULL, NULL);
  cleanup_qdisk(filename, qdisk);
}

Test(qdisk, allow_writing_more_than_max_size_when_last_message_does_not_fit)
{
  const gchar *filename = "test_qdisk_exceed_max_size.rqf";
  gsize qdisk_size = MiB(1);
  QDisk *qdisk = create_qdisk(TDISKQ_RELIABLE, filename, qdisk_size);
  qdisk_start(qdisk, NULL, NULL, NULL);

  push_dummy_record(qdisk, 100);

  cr_assert(push_dummy_record(qdisk, MiB(2)),
            "It should be allowed to overfill qdisk when the last message does not fit");

  cr_assert_geq(qdisk_get_file_size(qdisk), qdisk_get_maximum_size(qdisk));

  qdisk_stop(qdisk, NULL, NULL, NULL);
  cleanup_qdisk(filename, qdisk);
}

Test(qdisk, do_not_allow_diskq_to_exceed_max_size_if_last_message_fits)
{
  const gchar *filename = "test_qdisk_do_not_exceed_max_size_when_msg_fits.rqf";
  gsize qdisk_size = MiB(1);
  GString *data = g_string_new(NULL);
  QDisk *qdisk = create_qdisk(TDISKQ_RELIABLE, filename, qdisk_size);
  qdisk_start(qdisk, NULL, NULL, NULL);

  // fill completely
  push_dummy_record(qdisk, qdisk_size - QDISK_RESERVED_SPACE - FRAME_LENGTH);
  cr_assert_eq(qdisk_get_writer_head(qdisk), qdisk_get_maximum_size(qdisk));
  cr_assert_eq(qdisk_get_file_size(qdisk), qdisk_get_maximum_size(qdisk));

  cr_assert_not(push_dummy_record(qdisk, 1));

  reliable_pop_record_without_backlog(qdisk, data);

  push_dummy_record(qdisk, 4);
  cr_assert_leq(qdisk_get_file_size(qdisk), qdisk_get_maximum_size(qdisk));

  qdisk_stop(qdisk, NULL, NULL, NULL);
  g_string_free(data, TRUE);
  cleanup_qdisk(filename, qdisk);
}

Test(qdisk, completely_full_and_then_emptied_qdisk_should_update_positions_properly)
{
  const gchar *filename = "test_qdisk_completely_full.rqf";
  gsize qdisk_size = MiB(1);
  GString *popped_data = g_string_new(NULL);
  QDisk *qdisk = create_qdisk(TDISKQ_RELIABLE, filename, qdisk_size);
  qdisk_start(qdisk, NULL, NULL, NULL);

  gsize num_of_records = 4;

  /* to be able to fill completely, not even leaving one single byte of space */
  gsize record_len = ((qdisk_size - QDISK_RESERVED_SPACE) / num_of_records) - FRAME_LENGTH;

  // fill completely
  for (gsize i = 0; i < num_of_records; ++i)
    cr_assert(push_dummy_record(qdisk, record_len));

  // pop all
  for (gsize i = 0; i < num_of_records; ++i)
    cr_assert(reliable_pop_record_without_backlog(qdisk, popped_data));

  cr_assert(push_dummy_record(qdisk, record_len));
  cr_assert(reliable_pop_record_without_backlog(qdisk, popped_data));

  qdisk_stop(qdisk, NULL, NULL, NULL);
  g_string_free(popped_data, TRUE);
  cleanup_qdisk(filename, qdisk);
}

Test(qdisk, prealloc)
{
  const gchar *filename = "test_prealloc.rqf";

  DiskQueueOptions *opts = construct_diskq_options(TDISKQ_RELIABLE, MIN_CAPACITY_BYTES);
  disk_queue_options_set_prealloc(opts, TRUE);
  QDisk *qdisk = qdisk_new(opts, "TEST", filename);

  qdisk_start(qdisk, NULL, NULL, NULL);

  struct stat file_stats;
  cr_assert(stat(filename, &file_stats) == 0, "Stat call failed, errno: %d", errno);
  gint64 real_size = file_stats.st_size;

  cr_assert_eq(qdisk_get_file_size(qdisk), MIN_CAPACITY_BYTES);
  cr_assert_eq(qdisk_get_file_size(qdisk), real_size);

  qdisk_stop(qdisk, NULL, NULL, NULL);
  cleanup_qdisk(filename, qdisk);
}

static gboolean
_serialize_len_of_zeroes(SerializeArchive *sa, gpointer user_data)
{
  const gsize len = GPOINTER_TO_UINT(user_data);
  gchar *data = g_malloc0(len);

  gint result = serialize_archive_write_bytes(sa, data, len);

  g_free(data);
  return result;
}

static void
_push_data_to_qdisk(QDisk *qdisk, gsize len)
{
  cr_assert(len > sizeof(guint32));

  gsize len_to_serialize = len - sizeof(guint32);
  GString *buffer = g_string_new(NULL);
  GError *error = NULL;

  cr_assert(qdisk_serialize(buffer, _serialize_len_of_zeroes, GUINT_TO_POINTER(len_to_serialize), &error));
  cr_assert(qdisk_push_tail(qdisk, buffer));

  g_string_free(buffer, TRUE);
}

static void
_pop_and_ack(QDisk *qdisk)
{
  GString *buffer = g_string_new(NULL);

  cr_assert(qdisk_pop_head(qdisk, buffer));
  cr_assert(qdisk_ack_backlog(qdisk));

  g_string_free(buffer, TRUE);
}

static void
_assert_backlog_and_write_head_pos(QDisk *qdisk, gint64 backlog_head_pos, gint64 write_head_pos)
{
  cr_assert_eq(qdisk_get_backlog_head(qdisk), QDISK_RESERVED_SPACE + backlog_head_pos,
               "Backlog head positions does not match. Expected: %"G_GINT64_FORMAT" Actual: %"G_GINT64_FORMAT,
               QDISK_RESERVED_SPACE + backlog_head_pos, qdisk_get_backlog_head(qdisk));
  cr_assert_eq(qdisk_get_writer_head(qdisk), QDISK_RESERVED_SPACE + write_head_pos,
               "Write head positions does not match. %"G_GINT64_FORMAT" Actual: %"G_GINT64_FORMAT,
               QDISK_RESERVED_SPACE + backlog_head_pos, qdisk_get_writer_head(qdisk));
}

Test(qdisk, get_empty_space_non_wrapped)
{
  const gsize small_amount_of_data = 32;
  const gsize useful_size = MIN_CAPACITY_BYTES - QDISK_RESERVED_SPACE;

  const gchar *filename = "test_get_empty_space_non_wrapped.rqf";
  DiskQueueOptions *opts = construct_diskq_options(TDISKQ_RELIABLE, MIN_CAPACITY_BYTES);
  disk_queue_options_set_truncate_size_ratio(opts, 1);
  QDisk *qdisk = qdisk_new(opts, "TEST", filename);
  cr_assert(qdisk_start(qdisk, NULL, NULL, NULL));

  // 0   RESERVED=B=W              DBS
  // |---|------- ... -------------|
  //      ^^^^^^^^^^^^^^^^^^^^^^^^^
  _assert_backlog_and_write_head_pos(qdisk, 0, 0);
  cr_assert_eq(qdisk_get_empty_space(qdisk), useful_size);

  _push_data_to_qdisk(qdisk, small_amount_of_data);
  // 0   RESERVED=B      W         DBS
  // |---|--- ... -------|---------|
  //                      ^^^^^^^^^
  _assert_backlog_and_write_head_pos(qdisk, 0, small_amount_of_data);
  cr_assert_eq(qdisk_get_empty_space(qdisk), useful_size - small_amount_of_data);

  _pop_and_ack(qdisk);
  // 0   RESERVED           B=W    DBS
  // |---|------- ... ------|------|
  //      ^^^^^^^^^^^^^^^^^^^^^^^^^
  _assert_backlog_and_write_head_pos(qdisk, small_amount_of_data, small_amount_of_data);
  cr_assert_eq(qdisk_get_empty_space(qdisk), useful_size);

  qdisk_reset_file_if_empty(qdisk);
  _push_data_to_qdisk(qdisk, small_amount_of_data);
  _push_data_to_qdisk(qdisk, useful_size - small_amount_of_data);
  _pop_and_ack(qdisk);
  // 0   RESERVED      B           DBS=W
  // |---|---- ... ----|-----------|
  //      ^^^^^^^^^^^^^
  _assert_backlog_and_write_head_pos(qdisk, small_amount_of_data, useful_size);
  cr_assert_eq(qdisk_get_empty_space(qdisk), small_amount_of_data);
  _pop_and_ack(qdisk);
  qdisk_reset_file_if_empty(qdisk);

  _push_data_to_qdisk(qdisk, small_amount_of_data);
  _push_data_to_qdisk(qdisk, useful_size);
  _pop_and_ack(qdisk);
  // 0   RESERVED      B           DBS    W
  // |---|---- ... ----|-----------|------|
  //      ^^^^^^^^^^^^^
  _assert_backlog_and_write_head_pos(qdisk, small_amount_of_data, small_amount_of_data + useful_size);
  cr_assert_eq(qdisk_get_empty_space(qdisk), small_amount_of_data);
  _pop_and_ack(qdisk);
  qdisk_reset_file_if_empty(qdisk);

  _push_data_to_qdisk(qdisk, useful_size);
  _pop_and_ack(qdisk);
  // 0   RESERVED                  DBS=B=W
  // |---|------ ... --------------|
  //      ^^^^^^^^^^^^^^^^^^^^^^^^^
  _assert_backlog_and_write_head_pos(qdisk, useful_size, useful_size);
  cr_assert_eq(qdisk_get_empty_space(qdisk), useful_size);
  qdisk_reset_file_if_empty(qdisk);

  _push_data_to_qdisk(qdisk, useful_size + small_amount_of_data);
  _pop_and_ack(qdisk);
  // 0   RESERVED                  DBS    B=W
  // |---|------ ... --------------|------|
  //      ^^^^^^^^^^^^^^^^^^^^^^^^^
  _assert_backlog_and_write_head_pos(qdisk, useful_size + small_amount_of_data, useful_size + small_amount_of_data);
  cr_assert_eq(qdisk_get_empty_space(qdisk), useful_size);
  qdisk_reset_file_if_empty(qdisk);

  // These cases cannot be achieved with recent qdisk logic, only with older versions:
  //
  // 0   RESERVED                  DBS   B   W
  // |---|------ ... --------------|-----|---|
  //      ^^^^^^^^^^^^^^^^^^^^^^^^^
  //
  // and
  //
  // 0   RESERVED                  DBS=B     W
  // |---|------ ... --------------|---------|
  //      ^^^^^^^^^^^^^^^^^^^^^^^^^

  cr_assert(qdisk_stop(qdisk, NULL, NULL, NULL));
  cleanup_qdisk(filename, qdisk);
}

Test(qdisk, get_empty_space_wrapped)
{
  const gsize small_amount_of_data = 32;
  const gsize useful_size = MIN_CAPACITY_BYTES - QDISK_RESERVED_SPACE;

  const gchar *filename = "test_get_empty_space_wrapped.rqf";
  DiskQueueOptions *opts = construct_diskq_options(TDISKQ_RELIABLE, MIN_CAPACITY_BYTES);
  disk_queue_options_set_truncate_size_ratio(opts, 1);
  QDisk *qdisk = qdisk_new(opts, "TEST", filename);
  cr_assert(qdisk_start(qdisk, NULL, NULL, NULL));

  _push_data_to_qdisk(qdisk, small_amount_of_data * 2);
  _push_data_to_qdisk(qdisk, useful_size);
  _pop_and_ack(qdisk);
  _push_data_to_qdisk(qdisk, small_amount_of_data);
  // 0   RESERVED    W   B         DBS   FS
  // |---|--- ... ---|---|---------|-----|
  //                  ^^^
  _assert_backlog_and_write_head_pos(qdisk, small_amount_of_data * 2, small_amount_of_data);
  cr_assert_eq(qdisk_get_empty_space(qdisk), small_amount_of_data);
  _pop_and_ack(qdisk);
  _pop_and_ack(qdisk);
  qdisk_reset_file_if_empty(qdisk);

  _push_data_to_qdisk(qdisk, useful_size);
  _pop_and_ack(qdisk);
  _push_data_to_qdisk(qdisk, small_amount_of_data);
  // 0   RESERVED      W           DBS=B
  // |---|---- ... ----|-----------|
  //                    ^^^^^^^^^^^
  _assert_backlog_and_write_head_pos(qdisk, useful_size, small_amount_of_data);
  cr_assert_eq(qdisk_get_empty_space(qdisk), useful_size - small_amount_of_data);
  _pop_and_ack(qdisk);
  qdisk_reset_file_if_empty(qdisk);

  _push_data_to_qdisk(qdisk, useful_size + small_amount_of_data);
  _pop_and_ack(qdisk);
  _push_data_to_qdisk(qdisk, small_amount_of_data);
  // 0   RESERVED      W           DBS   B
  // |---|---- ... ----|-----------|-----|
  //                    ^^^^^^^^^^^
  _assert_backlog_and_write_head_pos(qdisk, useful_size + small_amount_of_data, small_amount_of_data);
  cr_assert_eq(qdisk_get_empty_space(qdisk), useful_size - small_amount_of_data);
  _pop_and_ack(qdisk);
  qdisk_reset_file_if_empty(qdisk);

  // These cases cannot be achieved with recent qdisk logic, only with older versions:
  //
  // 0   RESERVED                  DBS   W   B
  // |---|------ ... --------------|-----|---|
  //
  // or
  //
  // 0   RESERVED                  DBS=W     B
  // |---|------ ... --------------|---------|

  cr_assert(qdisk_stop(qdisk, NULL, NULL, NULL));
  cleanup_qdisk(filename, qdisk);
}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(qdisk, .init = setup, .fini = teardown);
