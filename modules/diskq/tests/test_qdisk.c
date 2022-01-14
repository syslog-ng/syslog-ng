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
construct_diskq_options(TestDiskQType dq_type, gint64 disk_buf_size)
{
  DiskQueueOptions *opts = g_new0(DiskQueueOptions, 1);
  disk_queue_options_set_default_options(opts);
  disk_queue_options_disk_buf_size_set(opts, disk_buf_size);
  disk_queue_options_reliable_set(opts, dq_type);

  return opts;
}

static QDisk *
create_qdisk(const gchar *filename, TestDiskQType dq_type, gint64 disk_buf_size)
{
  DiskQueueOptions *opts = construct_diskq_options(TDISKQ_RELIABLE, disk_buf_size);
  QDisk *qdisk = qdisk_new(opts, "TEST");

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

  qdisk_set_backlog_head(qdisk, qdisk_get_head_position(qdisk));
  return TRUE;
}

Test(qdisk, test_qdisk_started)
{
  const gchar *filename = "test_qdisk_started.rqf";
  QDisk *qdisk = create_qdisk(filename, TDISKQ_RELIABLE, MiB(1));

  cr_assert_not(qdisk_started(qdisk));

  qdisk_start(qdisk, filename, NULL, NULL, NULL);
  cr_assert(qdisk_started(qdisk));

  qdisk_stop(qdisk);
  cr_assert_not(qdisk_started(qdisk));

  cleanup_qdisk(filename, qdisk);
}

Test(qdisk, qdisk_basic_push_pop)
{
  const gchar *filename = "test_qdisk_basic_push_pop.rqf";
  QDisk *qdisk = create_qdisk(filename, TDISKQ_RELIABLE, MiB(1));
  qdisk_start(qdisk, filename, NULL, NULL, NULL);

  guint expected_record_len = 128;
  cr_assert(push_dummy_record(qdisk, expected_record_len));
  cr_assert_eq(qdisk_get_length(qdisk), 1);

  GString *popped_data = g_string_new(NULL);
  cr_assert(reliable_pop_record_without_backlog(qdisk, popped_data));
  assert_dummy_record(popped_data, expected_record_len);
  g_string_free(popped_data, TRUE);

  cr_assert_eq(qdisk_get_length(qdisk), 0);

  qdisk_stop(qdisk);
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
