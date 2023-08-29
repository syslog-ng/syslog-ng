/*
 * Copyright (c) 2023 Attila Szakacs
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

#include <unistd.h>

#include <criterion/criterion.h>
#include "libtest/queue_utils_lib.h"
#include "test_diskq_tools.h"

#include "diskq.h"
#include "diskq-global-metrics.h"
#include "logqueue-disk.h"
#include "apphook.h"

static void
_setup_with_persist(const gchar *persist_file)
{
  app_startup();

  configuration = cfg_new_snippet();
  configuration->stats_options.level = STATS_LEVEL1;
  configuration->state = persist_state_new(persist_file);
  persist_state_start(configuration->state);
  diskq_global_metrics_init();
  cfg_init(configuration);
}

static void
_teardown_with_persist(const gchar *persist_file)
{
  persist_state_cancel(configuration->state);
  unlink(persist_file);
  cfg_free(configuration);
  app_shutdown();
}

static LogDestDriver *
_dummy_dd_new(gboolean reliable_disk_buffer, const gchar *dir)
{
  LogDestDriver *self = g_new0(LogDestDriver, 1);
  log_dest_driver_init_instance(self, configuration);

  DiskQDestPlugin *plugin = diskq_dest_plugin_new();
  DiskQueueOptions *options = diskq_get_options(plugin);
  disk_queue_options_capacity_bytes_set(options, MIN_CAPACITY_BYTES);
  disk_queue_options_front_cache_size_set(options, 1);
  disk_queue_options_reliable_set(options, reliable_disk_buffer);
  disk_queue_options_set_dir(options, dir);
  cr_assert(log_driver_add_plugin(&self->super, (LogDriverPlugin *) plugin));

  return self;
}

static gboolean
_serialize_func(SerializeArchive *sa, gpointer user_data)
{
  LogMessage *msg = (LogMessage *) user_data;
  return log_msg_serialize(msg, sa, 0);
}

static void
_push_one_message(LogQueue *q, gssize *memory_size, gssize *serialized_size)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  path_options.ack_needed = FALSE;
  path_options.flow_control_requested = TRUE;

  gchar data[1024] = {0};
  memset(data, 'x', sizeof(data) - 1);

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_MESSAGE, data, sizeof(data));
  log_msg_add_ack(msg, &path_options);

  log_queue_push_tail(q, msg, &path_options);

  if (memory_size)
    {
      *memory_size = log_msg_get_size(msg);
    }

  if (serialized_size)
    {
      GError *error = NULL;
      GString *buf = g_string_new(NULL);
      cr_assert(qdisk_serialize(buf, _serialize_func, msg, &error));
      *serialized_size = buf->len;
      g_string_free(buf, TRUE);
      g_error_free(error);
      /* Some counters have KiB precision, make it larger, so it does not diminish. */
      cr_assert(*serialized_size > 1024);
    }
}

static void
_assert_counters(LogQueue *queue, gsize queued, gsize memory_usage, gsize capacity, gsize disk_usage,
                 gsize disk_allocated, guint line)
{
  LogQueueDisk *queue_disk = (LogQueueDisk *) queue;

  gsize capacity_kib = capacity / 1024;
  gsize disk_usage_kib = disk_usage / 1024;
  gsize disk_allocated_kib = disk_allocated / 1024;

  cr_assert(queue->metrics.owned.queued_messages);
  cr_assert(queue->metrics.owned.memory_usage);
  cr_assert(queue_disk->metrics.capacity);
  cr_assert(queue_disk->metrics.disk_usage);
  cr_assert(stats_counter_get(queue->metrics.owned.queued_messages) == queued,
            "line %d: Queued message counter mismatch. Expected: %lu Actual: %lu",
            line, queued, stats_counter_get(queue->metrics.owned.queued_messages));
  cr_assert(stats_counter_get(queue->metrics.owned.memory_usage) == memory_usage,
            "line %d: Memory usage counter mismatch. Expected: %lu Actual: %lu",
            line, memory_usage, stats_counter_get(queue->metrics.owned.memory_usage));
  cr_assert(stats_counter_get(queue_disk->metrics.capacity) == capacity_kib,
            "line %d: Capacity counter mismatch. Expected: %lu Actual: %lu",
            line, capacity_kib, stats_counter_get(queue_disk->metrics.capacity));
  cr_assert(stats_counter_get(queue_disk->metrics.disk_usage) == disk_usage_kib,
            "line %d: Disk usage counter mismatch. Expected: %lu Actual: %lu",
            line, disk_usage_kib, stats_counter_get(queue_disk->metrics.disk_usage));
  cr_assert(stats_counter_get(queue_disk->metrics.disk_allocated) == disk_allocated_kib,
            "line %d: Disk allocated counter mismatch. Expected: %lu Actual: %lu",
            line, disk_allocated_kib, stats_counter_get(queue_disk->metrics.disk_allocated));
}

static void
_test_non_reliable_setup(void)
{
  _setup_with_persist("test_non_reliable.persist");
}

static void
_test_non_reliable_teardown(void)
{
  _teardown_with_persist("test_non_reliable.persist");
}

Test(diskq_counters, test_non_reliable,
     .init = _test_non_reliable_setup,
     .fini = _test_non_reliable_teardown)
{
  const gsize expected_capacity = MIN_CAPACITY_BYTES - QDISK_RESERVED_SPACE;
  const gchar *queue_persist_name = "test_non_reliable";
  const gchar *dir_name = "test_diskq_counters_test_non_reliable";
  const gchar *dirlock_path = "test_diskq_counters_test_non_reliable/syslog-ng-disk-buffer.dirlock";

  StatsClusterKeyBuilder *queue_sck_builder = stats_cluster_key_builder_new();
  LogDestDriver *driver = _dummy_dd_new(FALSE, dir_name);
  cr_assert(log_pipe_init(&driver->super.super));

  /* Init empty disk-queue */
  LogQueue *queue = log_queue_ref(log_dest_driver_acquire_queue(driver, queue_persist_name, STATS_LEVEL0, NULL,
                                  queue_sck_builder));
  cr_assert(queue);
  _assert_counters(queue, 0, 0, expected_capacity, 0, QDISK_RESERVED_SPACE, __LINE__);

  /* First message goes to front cache */
  gssize one_message_memory_size;
  gssize one_message_serialized_size;
  _push_one_message(queue, &one_message_memory_size, &one_message_serialized_size);
  _assert_counters(queue, 1, one_message_memory_size, expected_capacity, 0, QDISK_RESERVED_SPACE, __LINE__);

  /* Second message goes to qdisk */
  _push_one_message(queue, NULL, NULL);
  _assert_counters(queue, 2, one_message_memory_size, expected_capacity, one_message_serialized_size,
                   QDISK_RESERVED_SPACE + one_message_serialized_size, __LINE__);

  /* Release and reacquire queue, same counters are expected */
  log_dest_driver_release_queue(driver, queue);

  stats_cluster_key_builder_free(queue_sck_builder);
  queue_sck_builder = stats_cluster_key_builder_new();
  queue = log_queue_ref(log_dest_driver_acquire_queue(driver, queue_persist_name, STATS_LEVEL0, NULL,
                                                      queue_sck_builder));
  cr_assert(queue);
  _assert_counters(queue, 2, one_message_memory_size, expected_capacity, one_message_serialized_size,
                   QDISK_RESERVED_SPACE + one_message_serialized_size, __LINE__);

  /* Pop one message, disk usage should reduce */
  send_some_messages(queue, 1, TRUE);
  _assert_counters(queue, 1, one_message_memory_size, expected_capacity, 0,
                   QDISK_RESERVED_SPACE + one_message_serialized_size, __LINE__);

  /* Pop one message, memory usage should reduce */
  send_some_messages(queue, 1, TRUE);
  _assert_counters(queue, 0, 0, expected_capacity, 0, QDISK_RESERVED_SPACE + one_message_serialized_size, __LINE__);

  stats_cluster_key_builder_free(queue_sck_builder);
  gchar *filename = g_strdup(log_queue_disk_get_filename(queue));
  log_dest_driver_release_queue(driver, queue);
  unlink(filename);
  unlink(dirlock_path);
  g_free(filename);
  rmdir(dir_name);
  cr_assert(log_pipe_unref(&driver->super.super));
}

static void
_test_reliable_setup(void)
{
  _setup_with_persist("test_reliable.persist");
}

static void
_test_reliable_teardown(void)
{
  _teardown_with_persist("test_reliable.persist");
}

Test(diskq_counters, test_reliable,
     .init = _test_reliable_setup,
     .fini = _test_reliable_teardown)
{
  const gsize expected_capacity = MIN_CAPACITY_BYTES - QDISK_RESERVED_SPACE;
  const gchar *queue_persist_name = "test_reliable";
  const gchar *dir_name = "test_diskq_counters_test_reliable";
  const gchar *dirlock_path = "test_diskq_counters_test_reliable/syslog-ng-disk-buffer.dirlock";

  StatsClusterKeyBuilder *queue_sck_builder = stats_cluster_key_builder_new();
  LogDestDriver *driver = _dummy_dd_new(TRUE, dir_name);
  cr_assert(log_pipe_init(&driver->super.super));

  /* Init empty disk-queue */
  LogQueue *queue = log_queue_ref(log_dest_driver_acquire_queue(driver, queue_persist_name, STATS_LEVEL0, NULL,
                                  queue_sck_builder));
  cr_assert(queue);
  _assert_counters(queue, 0, 0, expected_capacity, 0, QDISK_RESERVED_SPACE, __LINE__);

  /* The message goes to both front cache and qdisk */
  gssize one_message_memory_size;
  gssize one_message_serialized_size;
  _push_one_message(queue, &one_message_memory_size, &one_message_serialized_size);
  _assert_counters(queue, 1, one_message_memory_size, expected_capacity, one_message_serialized_size,
                   QDISK_RESERVED_SPACE + one_message_serialized_size, __LINE__);

  /* Release and reacquire queue, only disk usage is expected, as front cache is not refilled */
  log_dest_driver_release_queue(driver, queue);

  stats_cluster_key_builder_free(queue_sck_builder);
  queue_sck_builder = stats_cluster_key_builder_new();
  queue = log_queue_ref(log_dest_driver_acquire_queue(driver, queue_persist_name, STATS_LEVEL0, NULL,
                                                      queue_sck_builder));
  cr_assert(queue);
  _assert_counters(queue, 1, 0, expected_capacity, one_message_serialized_size,
                   QDISK_RESERVED_SPACE + one_message_serialized_size, __LINE__);

  /* Pop the message, disk usage should reduce */
  send_some_messages(queue, 1, TRUE);
  _assert_counters(queue, 0, 0, expected_capacity, 0, QDISK_RESERVED_SPACE + one_message_serialized_size, __LINE__);

  stats_cluster_key_builder_free(queue_sck_builder);
  gchar *filename = g_strdup(log_queue_disk_get_filename(queue));
  log_dest_driver_release_queue(driver, queue);
  unlink(filename);
  unlink(dirlock_path);
  g_free(filename);
  rmdir(dir_name);
  cr_assert(log_pipe_unref(&driver->super.super));
}

TestSuite(diskq_counters);
