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

#include "apphook.h"
#include "cfg.h"
#include "diskq-config.h"
#include "diskq-global-metrics.h"
#include "logqueue-disk-non-reliable.h"
#include "logqueue-disk-reliable.h"
#include "mainloop.h"
#include "messages.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster-single.h"
#include "timeutils/misc.h"
#include "qdisk.h"

#include <errno.h>
#include <iv.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

#define B_TO_KiB(x) ((x) / 1024)

typedef struct DiskQGlobalMetrics_
{
  GMutex lock;
  struct iv_timer dir_watch_timer;

  GHashTable *dirs;
  gint freq;
} DiskQGlobalMetrics;

static DiskQGlobalMetrics diskq_global_metrics;
static gboolean diskq_global_metrics_global_inited;

static void
_dir_watch_timer_stop(DiskQGlobalMetrics *self)
{
  if (iv_timer_registered(&self->dir_watch_timer))
    iv_timer_unregister(&self->dir_watch_timer);
}

static void
_dir_watch_timer_start(DiskQGlobalMetrics *self)
{
  _dir_watch_timer_stop(self);

  iv_validate_now();
  self->dir_watch_timer.expires = iv_now;
  timespec_add_msec(&self->dir_watch_timer.expires, self->freq * 1000);
  iv_timer_register(&self->dir_watch_timer);
}

/* Must be called with the lock held. */
static void
_track_acquired_file(GHashTable *tracked_files, const gchar *filename)
{
  g_hash_table_insert(tracked_files, g_strdup(filename), GINT_TO_POINTER((gint) TRUE));
}

/* Must be called with the lock held. */
static void
_track_released_file(GHashTable *tracked_files, const gchar *filename)
{
  g_hash_table_insert(tracked_files, g_strdup(filename), GINT_TO_POINTER((gint) FALSE));
}

/* Must be called with the lock held. */
static gboolean
_is_file_tracked(GHashTable *tracked_files, const gchar *filename)
{
  return g_hash_table_contains(tracked_files, filename);
}

static gboolean
_file_exists_and_non_empty(const gchar *dir, const gchar *filename)
{
  gchar *abs_filename = g_build_filename(dir, filename, NULL);

  struct stat st;
  gboolean result = stat(abs_filename, &st) >= 0 && st.st_size > 0;

  g_free(abs_filename);
  return result;
}

static gboolean
_is_non_corrupted_disk_buffer_file(const gchar *dir, const gchar *filename)
{
  return qdisk_is_file_a_disk_buffer_file(filename) && strstr(filename, "corrupted") == NULL &&
         _file_exists_and_non_empty(dir, filename);
}

/* Must be called with the lock held. */
static void
_init_abandoned_disk_buffer_sc_keys(StatsClusterKey *queued_sc_key, StatsClusterKey *capacity_sc_key,
                                    StatsClusterKey *disk_allocated_sc_key, StatsClusterKey *disk_usage_sc_key,
                                    const gchar *abs_filename, gboolean reliable)
{
  enum { labels_len = 3 };
  static StatsClusterLabel labels[labels_len];
  labels[0] = stats_cluster_label("abandoned", "true");
  labels[1] = stats_cluster_label("path", abs_filename);
  labels[2] = stats_cluster_label("reliable", reliable ? "true" : "false");

  stats_cluster_single_key_set(queued_sc_key, "disk_queue_events", labels, labels_len);

  stats_cluster_single_key_set(capacity_sc_key, "disk_queue_capacity_bytes", labels, labels_len);
  stats_cluster_single_key_add_unit(capacity_sc_key, SCU_KIB);

  stats_cluster_single_key_set(disk_allocated_sc_key, "disk_queue_disk_allocated_bytes", labels, labels_len);
  stats_cluster_single_key_add_unit(disk_allocated_sc_key, SCU_KIB);

  stats_cluster_single_key_set(disk_usage_sc_key, "disk_queue_disk_usage_bytes", labels, labels_len);
  stats_cluster_single_key_add_unit(disk_usage_sc_key, SCU_KIB);
}

static void
_init_disk_queue_options(DiskQueueOptions *options, const gchar *dir, const gchar *filename)
{
  disk_queue_options_set_default_options(options);
  disk_queue_options_set_dir(options, dir);
  g_assert(qdisk_is_disk_buffer_file_reliable(filename, &options->reliable));
  options->read_only = TRUE;
}

static LogQueueDisk *
_create_log_queue_disk(DiskQueueOptions *options, const gchar *abs_filename)
{
  if (options->reliable)
    return (LogQueueDisk *) log_queue_disk_reliable_new(options, abs_filename, NULL, STATS_LEVEL0, NULL, NULL);
  else
    return (LogQueueDisk *) log_queue_disk_non_reliable_new(options, abs_filename, NULL, STATS_LEVEL0, NULL, NULL);
}

/* Must be called with the lock held. */
static void
_set_abandoned_disk_buffer_file_metrics(const gchar *dir, const gchar *filename)
{
  DiskQueueOptions options;
  _init_disk_queue_options(&options, dir, filename);
  gchar *abs_filename = g_build_filename(dir, filename, NULL);
  LogQueueDisk *queue = _create_log_queue_disk(&options, abs_filename);

  if (!log_queue_disk_start(&queue->super))
    {
      log_queue_unref(&queue->super);
      disk_queue_options_destroy(&options);
      g_free(abs_filename);
      return;
    }

  StatsCounterItem *queued, *capacity, *disk_allocated, *disk_usage;
  StatsCluster *queued_c, *capacity_c, *disk_allocated_c, *disk_usage_c;
  StatsClusterKey queued_sc_key, capacity_sc_key, disk_allocated_sc_key, disk_usage_sc_key;
  _init_abandoned_disk_buffer_sc_keys(&queued_sc_key, &capacity_sc_key, &disk_allocated_sc_key, &disk_usage_sc_key,
                                      abs_filename, options.reliable);

  stats_lock();
  {
    queued_c = stats_register_dynamic_counter(STATS_LEVEL1, &queued_sc_key, SC_TYPE_SINGLE_VALUE, &queued);
    capacity_c = stats_register_dynamic_counter(STATS_LEVEL1, &capacity_sc_key, SC_TYPE_SINGLE_VALUE, &capacity);
    disk_allocated_c = stats_register_dynamic_counter(STATS_LEVEL1, &disk_allocated_sc_key, SC_TYPE_SINGLE_VALUE,
                                                      &disk_allocated);
    disk_usage_c = stats_register_dynamic_counter(STATS_LEVEL1, &disk_usage_sc_key, SC_TYPE_SINGLE_VALUE, &disk_usage);

    stats_counter_set(queued, log_queue_get_length(&queue->super));
    stats_counter_set(capacity, B_TO_KiB(qdisk_get_max_useful_space(queue->qdisk)));
    stats_counter_set(disk_allocated, B_TO_KiB(qdisk_get_file_size(queue->qdisk)));
    stats_counter_set(disk_usage, B_TO_KiB(qdisk_get_used_useful_space(queue->qdisk)));

    stats_unregister_dynamic_counter(queued_c, SC_TYPE_SINGLE_VALUE, &queued);
    stats_unregister_dynamic_counter(capacity_c, SC_TYPE_SINGLE_VALUE, &capacity);
    stats_unregister_dynamic_counter(disk_allocated_c, SC_TYPE_SINGLE_VALUE, &disk_allocated);
    stats_unregister_dynamic_counter(disk_usage_c, SC_TYPE_SINGLE_VALUE, &disk_usage);
  }
  stats_unlock();

  gboolean persistent;
  log_queue_disk_stop(&queue->super, &persistent);
  log_queue_unref(&queue->super);
  disk_queue_options_destroy(&options);
  g_free(abs_filename);
}

/* Must be called with the lock held. */
static void
_unset_abandoned_disk_buffer_file_metrics(const gchar *dir, const gchar *filename)
{
  gchar *abs_filename = g_build_filename(dir, filename, NULL);
  gboolean reliable;
  g_assert(qdisk_is_disk_buffer_file_reliable(filename, &reliable));

  StatsClusterKey queued_sc_key, capacity_sc_key, disk_allocated_sc_key, disk_usage_sc_key;
  _init_abandoned_disk_buffer_sc_keys(&queued_sc_key, &capacity_sc_key, &disk_allocated_sc_key, &disk_usage_sc_key,
                                      abs_filename, reliable);

  stats_lock();
  {
    stats_remove_cluster(&queued_sc_key);
    stats_remove_cluster(&capacity_sc_key);
    stats_remove_cluster(&disk_allocated_sc_key);
    stats_remove_cluster(&disk_usage_sc_key);
  }
  stats_unlock();

  g_free(abs_filename);
}

/* Must be called with the lock held. */
static void
_track_disk_buffer_files_in_dir(const gchar *dir, GHashTable *tracked_files)
{
  DIR *dir_stream = opendir(dir);
  if (!dir_stream)
    {
      msg_debug("disk-buffer: Failed to list files in dir",
                evt_tag_str("dir", dir),
                evt_tag_str("error", g_strerror(errno)));
      return;
    }

  while (TRUE)
    {
      struct dirent *dir_entry = readdir(dir_stream);
      if (!dir_entry)
        break;

      const gchar *filename = dir_entry->d_name;
      if (_is_file_tracked(tracked_files, filename) || !_is_non_corrupted_disk_buffer_file(dir, filename))
        continue;

      _track_released_file(tracked_files, filename);
      _set_abandoned_disk_buffer_file_metrics(dir, filename);
    }

  closedir(dir_stream);
}

static gboolean
_get_available_space_mib_in_dir(const gchar *dir, gint64 *available_space_mib)
{
  struct statvfs stat;
  if (statvfs(dir, &stat) < 0 )
    {
      msg_debug("disk-buffer: Failed to get filesystem info",
                evt_tag_str("dir", dir),
                evt_tag_str("error", g_strerror(errno)));
      return FALSE;
    }

  *available_space_mib = stat.f_bsize * stat.f_bavail / (1024 * 1024);

  return TRUE;
}

/* Must be called with the lock held. */
static void
_init_dir_sc_keys(StatsClusterKey *available_bytes_sc_key, const gchar *dir)
{
  enum { labels_len = 1 };
  static StatsClusterLabel labels[labels_len];
  labels[0] = stats_cluster_label("dir", dir);

  stats_cluster_single_key_set(available_bytes_sc_key, "disk_queue_dir_available_bytes", labels, G_N_ELEMENTS(labels));
  /* Up to 4096 TiB with 32 bit atomic counters. */
  stats_cluster_single_key_add_unit(available_bytes_sc_key, SCU_MIB);
}

/* Must be called with the lock held. */
static void
_update_dir_metrics(gpointer key, gpointer value, gpointer user_data)
{
  const gchar *dir = (const gchar *) key;

  gint64 available_space_mib;
  if (!_get_available_space_mib_in_dir(dir, &available_space_mib))
    return;

  StatsClusterKey available_bytes_sc_key;
  _init_dir_sc_keys(&available_bytes_sc_key, dir);

  stats_lock();
  {
    StatsCounterItem *counter;
    StatsCluster *cluster = stats_register_dynamic_counter(STATS_LEVEL1, &available_bytes_sc_key, SC_TYPE_SINGLE_VALUE,
                                                           &counter);
    stats_counter_set(counter, available_space_mib);
    stats_unregister_dynamic_counter(cluster, SC_TYPE_SINGLE_VALUE, &counter);
  }
  stats_unlock();
}

static void
_update_all_dir_metrics(gpointer s)
{
  DiskQGlobalMetrics *self = (DiskQGlobalMetrics *) s;

  g_mutex_lock(&self->lock);
  {
    g_hash_table_foreach(self->dirs, _update_dir_metrics, NULL);
  }
  g_mutex_unlock(&self->lock);

  _dir_watch_timer_start(self);
}

/* Must be called with the lock held. */
static void
_unset_dir_metrics(const gchar *dir)
{
  StatsClusterKey available_bytes_sc_key;
  _init_dir_sc_keys(&available_bytes_sc_key, dir);

  stats_lock();
  {
    stats_remove_cluster(&available_bytes_sc_key);
  }
  stats_unlock();
}

/* Must be called with the lock held. */
static void
_unset_abandoned_disk_buffer_file_metrics_foreach_fn(gpointer key, gpointer value, gpointer user_data)
{
  const gchar *filename = (const gchar *) key;
  gboolean acquired = (gboolean) GPOINTER_TO_INT(value);
  const gchar *dir = (const gchar *) user_data;

  if (acquired)
    return;

  _unset_abandoned_disk_buffer_file_metrics(dir, filename);
}

/* Must be called with the lock held. */
static void
_unset_all_metrics_in_dir(gpointer key, gpointer value, gpointer user_data)
{
  gchar *dir = (gchar *) key;
  GHashTable *tracked_files = (GHashTable *) value;

  _unset_dir_metrics(dir);
  g_hash_table_foreach(tracked_files, _unset_abandoned_disk_buffer_file_metrics_foreach_fn, dir);
}

static void
_new(gint type, gpointer c)
{
  DiskQGlobalMetrics *self = &diskq_global_metrics;

  IV_TIMER_INIT(&self->dir_watch_timer);
  self->dir_watch_timer.handler = _update_all_dir_metrics;
  self->dir_watch_timer.cookie = self;

  self->dirs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_hash_table_destroy);
}

static void
_init(gint type, gpointer c)
{
  GlobalConfig *cfg = main_loop_get_current_config(main_loop_get_instance());
  if (!cfg)
    return;

  DiskQGlobalMetrics *self = &diskq_global_metrics;

  self->freq = disk_queue_config_get_stats_freq(cfg);
  if (self->freq == 0)
    return;

  _update_all_dir_metrics(self);
}

static void
_deinit(gint type, gpointer c)
{
  DiskQGlobalMetrics *self = &diskq_global_metrics;

  _dir_watch_timer_stop(self);

  g_mutex_lock(&self->lock);
  {
    g_hash_table_foreach(self->dirs, _unset_all_metrics_in_dir, NULL);
    g_hash_table_remove_all(self->dirs);
  }
  g_mutex_unlock(&self->lock);
}

static void
_free(gint type, gpointer c)
{
  DiskQGlobalMetrics *self = &diskq_global_metrics;

  g_hash_table_destroy(self->dirs);
}

void
diskq_global_metrics_init(void)
{
  if (diskq_global_metrics_global_inited)
    return;

  register_application_hook(AH_STARTUP, _new, NULL, AHM_RUN_ONCE);
  register_application_hook(AH_CONFIG_CHANGED, _init, NULL, AHM_RUN_REPEAT);
  register_application_hook(AH_CONFIG_STOPPED, _deinit, NULL, AHM_RUN_REPEAT);
  register_application_hook(AH_SHUTDOWN, _free, NULL, AHM_RUN_ONCE);

  diskq_global_metrics_global_inited = TRUE;
}

void
diskq_global_metrics_file_acquired(const gchar *abs_filename)
{
  DiskQGlobalMetrics *self = &diskq_global_metrics;

  gchar *dir = g_path_get_dirname(abs_filename);
  gchar *filename = g_path_get_basename(abs_filename);

  g_mutex_lock(&self->lock);
  {
    GHashTable *tracked_files = g_hash_table_lookup(self->dirs, dir);
    if (!tracked_files)
      {
        tracked_files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
        _track_disk_buffer_files_in_dir(dir, tracked_files);
        g_hash_table_insert(self->dirs, g_strdup(dir), tracked_files);
      }

    _track_acquired_file(tracked_files, filename);
    _unset_abandoned_disk_buffer_file_metrics(dir, filename);
  }
  g_mutex_unlock(&self->lock);

  g_free(filename);
  g_free(dir);
}

void
diskq_global_metrics_file_released(const gchar *abs_filename)
{
  DiskQGlobalMetrics *self = &diskq_global_metrics;

  gchar *dir = g_path_get_dirname(abs_filename);
  gchar *filename = g_path_get_basename(abs_filename);

  g_mutex_lock(&self->lock);
  {
    GHashTable *tracked_files = g_hash_table_lookup(self->dirs, dir);
    g_assert(tracked_files);

    if (_is_non_corrupted_disk_buffer_file(dir, filename))
      {
        _track_released_file(tracked_files, filename);
        _set_abandoned_disk_buffer_file_metrics(dir, filename);
      }
  }
  g_mutex_unlock(&self->lock);

  g_free(filename);
  g_free(dir);
}
