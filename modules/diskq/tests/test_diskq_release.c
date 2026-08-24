/*
 * Copyright (c) 2026 One Identity LLC.
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
#include <sys/stat.h>
#include <glib/gstdio.h>

#include "libtest/queue_utils_lib.h"

#include "diskq.h"
#include "diskq-global-metrics.h"
#include "logqueue-disk.h"
#include "apphook.h"
#include "persist-state.h"

#define PERSIST_FILE "test_diskq_release.persist"
#define QUEUE_PERSIST_NAME "test_diskq_release_queue"

static gchar *base_dir;
static gchar *workspace_dir;

static void
setup(void)
{
  app_startup();

  base_dir = g_get_current_dir();
  workspace_dir = NULL;

  configuration = cfg_new_snippet();
  configuration->stats_options.level = STATS_LEVEL1;
  diskq_global_metrics_init();
}

/* the queue file, the persist file and the dirlock all land in the workspace,
 * and distcheck fails on anything left in the build directory */
static void
_leave_workspace(void)
{
  cr_assert_eq(g_chdir(base_dir), 0, "failed to return to the build directory: %s", base_dir);

  if (!workspace_dir)
    return;

  GDir *dir = g_dir_open(workspace_dir, 0, NULL);
  cr_assert_not_null(dir, "failed to open the workspace: %s", workspace_dir);

  const gchar *entry;
  while ((entry = g_dir_read_name(dir)))
    {
      gchar *path = g_build_filename(workspace_dir, entry, NULL);
      cr_assert_eq(g_unlink(path), 0, "failed to remove a file left in the workspace: %s", path);
      g_free(path);
    }
  g_dir_close(dir);

  cr_assert_eq(g_rmdir(workspace_dir), 0, "failed to remove the workspace: %s", workspace_dir);

  g_free(workspace_dir);
  workspace_dir = NULL;
}

static void
teardown(void)
{
  if (configuration->state)
    persist_state_cancel(configuration->state);
  cfg_free(configuration);
  app_shutdown();

  _leave_workspace();
  g_free(base_dir);
}

TestSuite(diskq_release, .init = setup, .fini = teardown);

/* tests share the working directory of the binary, so each one needs its own
 * persist file and spool dir to survive a parallel run. the workspace is
 * created relative to the build directory rather than to the current one, so
 * that the suite does not nest its workspaces when Criterion runs without
 * forking and the process keeps the previous test's directory */
static void
_enter_workspace(const gchar *name)
{
  workspace_dir = g_build_filename(base_dir, name, NULL);

  cr_assert_eq(g_mkdir_with_parents(workspace_dir, 0700), 0, "failed to create the workspace: %s", workspace_dir);
  cr_assert_eq(g_chdir(workspace_dir), 0, "failed to enter the workspace: %s", workspace_dir);

  configuration->state = persist_state_new(PERSIST_FILE);
  cr_assert(persist_state_start(configuration->state), "failed to start the persist state");
  cr_assert(cfg_init(configuration), "failed to initialize the configuration");
}

static LogDestDriver *
_dummy_dd_new(gboolean reliable, gint front_cache_size, gboolean remove_if_empty)
{
  LogDestDriver *self = g_new0(LogDestDriver, 1);
  log_dest_driver_init_instance(self, configuration);

  DiskQDestPlugin *plugin = diskq_dest_plugin_new();
  DiskQueueOptions *options = diskq_get_options(plugin);
  disk_queue_options_capacity_bytes_set(options, MIN_CAPACITY_BYTES);
  disk_queue_options_reliable_set(options, reliable);
  disk_queue_options_front_cache_size_set(options, front_cache_size);
  disk_queue_options_set_dir(options, ".");
  disk_queue_options_set_remove_if_empty(options, remove_if_empty);
  cr_assert(log_driver_add_plugin(&self->super, (LogDriverPlugin *) plugin));
  cr_assert(log_pipe_init(&self->super.super));

  return self;
}

static void
_dummy_dd_free(LogDestDriver *self)
{
  cr_assert(log_pipe_deinit(&self->super.super));
  log_pipe_unref(&self->super.super);
}

static gboolean
_file_exists(const gchar *filename)
{
  struct stat st;
  return stat(filename, &st) == 0;
}

static gchar *
_persist_entry_value(void)
{
  gsize length = 0;
  guint8 version = 0;
  return persist_state_lookup_string(configuration->state, QUEUE_PERSIST_NAME, &length, &version);
}

/* removing an entry only clears its in_use flag and a lookup would set it
 * again, so the entry is only observable as gone after a commit and reload */
static gboolean
_entry_survives_a_reload(void)
{
  cr_assert(persist_state_commit(configuration->state), "failed to commit the persist state");
  persist_state_free(configuration->state);

  configuration->state = persist_state_new(PERSIST_FILE);
  cr_assert(persist_state_start(configuration->state), "failed to reload the persist state");

  gsize size = 0;
  guint8 version = 0;

  return persist_state_lookup_entry(configuration->state, QUEUE_PERSIST_NAME, &size, &version) != 0;
}

/* acquire a queue, optionally feed it, release it, report what became of its file */
static void
_acquire_feed_release(gboolean reliable, gint front_cache_size, gint messages, gboolean remove_if_empty,
                      gchar **filename, gboolean *file_survived, gboolean *entry_survived)
{
  LogDestDriver *driver = _dummy_dd_new(reliable, front_cache_size, remove_if_empty);

  /* releasing consumes two references: ours and the one on the driver's list */
  LogQueue *queue = log_queue_ref(log_dest_driver_acquire_queue(driver, QUEUE_PERSIST_NAME, STATS_LEVEL0, NULL, NULL));
  cr_assert_not_null(queue, "failed to acquire a disk-buffer queue");

  *filename = g_strdup(log_queue_disk_get_filename(queue));
  cr_assert(_file_exists(*filename), "the queue file was not created: %s", *filename);

  gchar *recorded = _persist_entry_value();
  cr_assert_not_null(recorded, "acquiring the queue recorded no persist entry");
  cr_assert_str_eq(recorded, *filename, "the persist entry names a different file than the queue uses");
  g_free(recorded);

  if (messages > 0)
    feed_some_messages(queue, messages);

  log_dest_driver_release_queue(driver, queue);

  *file_survived = _file_exists(*filename);

  _dummy_dd_free(driver);

  *entry_survived = _entry_survives_a_reload();
}

Test(diskq_release, test_empty_reliable_queue_file_is_removed_with_its_persist_entry)
{
  gchar *filename = NULL;
  gboolean file_survived, entry_survived;

  _enter_workspace("empty_reliable");
  _acquire_feed_release(TRUE, 0, 0, TRUE, &filename, &file_survived, &entry_survived);

  cr_assert_not(file_survived, "an empty reliable queue file was left behind: %s", filename);
  cr_assert_not(entry_survived, "the persist entry outlived the file it names");

  g_free(filename);
}

/* the option is opt-in, so without it an empty file is kept exactly as before */
Test(diskq_release, test_empty_queue_file_is_kept_without_remove_if_empty)
{
  gchar *filename = NULL;
  gboolean file_survived, entry_survived;

  _enter_workspace("empty_without_remove_if_empty");
  _acquire_feed_release(TRUE, 0, 0, FALSE, &filename, &file_survived, &entry_survived);

  cr_assert(file_survived, "an empty queue file was removed without remove-if-empty(yes): %s", filename);
  cr_assert(entry_survived, "the persist entry was dropped without remove-if-empty(yes)");

  g_free(filename);
}

Test(diskq_release, test_reliable_queue_file_holding_a_message_is_kept)
{
  gchar *filename = NULL;
  gboolean file_survived, entry_survived;

  _enter_workspace("reliable_with_message");
  _acquire_feed_release(TRUE, 0, 1, TRUE, &filename, &file_survived, &entry_survived);

  cr_assert(file_survived, "a reliable queue file holding a message was removed: %s", filename);
  cr_assert(entry_survived, "the persist entry for a kept file was dropped");

  g_free(filename);
}

Test(diskq_release, test_empty_non_reliable_queue_file_is_removed_with_its_persist_entry)
{
  gchar *filename = NULL;
  gboolean file_survived, entry_survived;

  _enter_workspace("empty_non_reliable");
  _acquire_feed_release(FALSE, 0, 0, TRUE, &filename, &file_survived, &entry_survived);

  cr_assert_not(file_survived, "an empty non-reliable queue file was left behind: %s", filename);
  cr_assert_not(entry_survived, "the persist entry outlived the file it names");

  g_free(filename);
}

Test(diskq_release, test_non_reliable_queue_file_holding_a_message_is_kept)
{
  gchar *filename = NULL;
  gboolean file_survived, entry_survived;

  _enter_workspace("non_reliable_with_message");
  _acquire_feed_release(FALSE, 0, 1, TRUE, &filename, &file_survived, &entry_survived);

  cr_assert(file_survived, "a non-reliable queue file holding a message was removed: %s", filename);
  cr_assert(entry_survived, "the persist entry for a kept file was dropped");

  g_free(filename);
}

/* the message is in the front cache, not in the qdisk part, so the qdisk length
 * is zero while the file is not empty. stopping serializes the cache into the
 * file, and the count that decides has to include it */
Test(diskq_release, test_non_reliable_queue_file_holding_only_a_front_cached_message_is_kept)
{
  gchar *filename = NULL;
  gboolean file_survived, entry_survived;

  _enter_workspace("non_reliable_front_cache");
  _acquire_feed_release(FALSE, 16, 1, TRUE, &filename, &file_survived, &entry_survived);

  cr_assert(file_survived,
            "a non-reliable queue file holding a front-cached message was removed: %s", filename);
  cr_assert(entry_survived, "the persist entry for a kept file was dropped");

  g_free(filename);
}
