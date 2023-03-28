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
#include "mainloop.h"
#include "messages.h"
#include "stats/stats-registry.h"
#include "stats/stats-cluster-single.h"
#include "timeutils/misc.h"

#include <iv.h>
#include <sys/statvfs.h>

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

static void
_track_acquired_file(GHashTable *tracked_files, const gchar *filename)
{
  g_hash_table_insert(tracked_files, g_strdup(filename), GINT_TO_POINTER((gint) TRUE));
}

static void
_track_released_file(GHashTable *tracked_files, const gchar *filename)
{
  g_hash_table_insert(tracked_files, g_strdup(filename), GINT_TO_POINTER((gint) FALSE));
}

static gboolean
_is_file_tracked(GHashTable *tracked_files, const gchar *filename)
{
  return g_hash_table_contains(tracked_files, filename);
}

static gboolean
_get_available_space_mib_in_dir(const gchar *dir, gint64 *available_space_mib)
{
  struct statvfs stat;
  if (statvfs(dir, &stat) < 0 )
    {
      msg_error("disk-buffer: Failed to get filesystem info",
                evt_tag_str("dir", dir),
                evt_tag_error("error"));
      return FALSE;
    }

  *available_space_mib = stat.f_bsize * stat.f_bavail / (1024 * 1024);

  return TRUE;
}

static void
_update_dir_metrics(gpointer key, gpointer value, gpointer user_data)
{
  const gchar *dir = (const gchar *) key;

  gint64 available_space_mib;
  if (!_get_available_space_mib_in_dir(dir, &available_space_mib))
    return;

  StatsClusterLabel labels[] = { stats_cluster_label("dir", dir), };
  StatsClusterKey sc_key;
  stats_cluster_single_key_set(&sc_key, "disk_queue_dir_available_bytes", labels, G_N_ELEMENTS(labels));
  /* Up to 4096 TiB with 32 bit atomic counters. */
  stats_cluster_single_key_add_unit(&sc_key, SCU_MIB);

  stats_lock();
  {
    StatsCounterItem *counter;
    StatsCluster *cluster = stats_register_dynamic_counter(STATS_LEVEL1, &sc_key, SC_TYPE_SINGLE_VALUE, &counter);
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
diskq_global_metrics_watch_dir(const gchar *dir)
{
  DiskQGlobalMetrics *self = &diskq_global_metrics;

  g_mutex_lock(&self->lock);
  {
    if (!g_hash_table_contains(self->dirs, dir))
      {
        GHashTable *tracked_files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
        g_hash_table_insert(self->dirs, g_strdup(dir), tracked_files);
      }
  }
  g_mutex_unlock(&self->lock);
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
    g_assert(tracked_files);

    _track_acquired_file(tracked_files, filename);
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

    _track_released_file(tracked_files, filename);
  }
  g_mutex_unlock(&self->lock);

  g_free(filename);
  g_free(dir);
}
