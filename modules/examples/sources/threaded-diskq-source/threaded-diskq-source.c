/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 László Várady <laszlo.varady@balabit.com>
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

#include "threaded-diskq-source.h"
#include "logthrsource/logthrfetcherdrv.h"
#include "logqueue-disk.h"
#include "logqueue-disk-reliable.h"
#include "logqueue-disk-non-reliable.h"
#include "logmsg/logmsg.h"
#include "messages.h"

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

struct ThreadedDiskqSourceDriver
{
  LogThreadedFetcherDriver super;
  DiskQueueOptions diskq_options;
  LogQueue *queue;

  struct stat diskq_file_stat;
  gboolean waiting_for_file_change;

  StatsClusterKeyBuilder *queue_sck_builder;
  gchar *filename;
};

static gboolean
_load_queue(ThreadedDiskqSourceDriver *self)
{
  self->diskq_options.read_only = TRUE;
  self->diskq_options.reliable = FALSE;

  FILE *file = fopen(self->filename, "rb");

  if (!file)
    {
      msg_error("Error opening diskq file", evt_tag_str("file", self->filename));
      return FALSE;
    }

  gchar file_signature[5] = {0};
  if (fread(file_signature, 4, 1, file) == 0)
    {
      msg_error("Error reading diskq file signature", evt_tag_str("file", self->filename));
      fclose(file);
      return FALSE;
    }

  fclose(file);

  if (strcmp(file_signature, "SLRQ") == 0)
    self->diskq_options.reliable = TRUE;

  if (self->diskq_options.reliable)
    {
      self->diskq_options.flow_control_window_bytes = 1024 * 1024;
      self->queue = log_queue_disk_reliable_new(&self->diskq_options, self->filename, NULL, STATS_LEVEL0, NULL,
                                                self->queue_sck_builder);
    }
  else
    {
      self->diskq_options.flow_control_window_bytes = 128;
      self->diskq_options.front_cache_size = 1000;
      self->queue = log_queue_disk_non_reliable_new(&self->diskq_options, self->filename, NULL, STATS_LEVEL0, NULL,
                                                    self->queue_sck_builder);
    }

  if (!log_queue_disk_start(self->queue))
    {
      msg_error("Error loading diskq", evt_tag_str("file", self->filename));
      return FALSE;
    }

  return TRUE;
}

static gboolean
_new_diskq_file_exists(ThreadedDiskqSourceDriver *self, const struct stat *new_diskq_file_stat)
{
  return new_diskq_file_stat->st_mtime != self->diskq_file_stat.st_mtime
         || new_diskq_file_stat->st_size != self->diskq_file_stat.st_size;
}

static gboolean
_open_diskq(LogThreadedFetcherDriver *s)
{
  ThreadedDiskqSourceDriver *self = (ThreadedDiskqSourceDriver *) s;

  struct stat new_diskq_file_stat;
  if (stat(self->filename, &new_diskq_file_stat) != 0)
    {
      msg_info("Diskq file does now exist, retrying", evt_tag_str("file", self->filename));
      return FALSE;
    }

  if (self->waiting_for_file_change)
    {
      if(!_new_diskq_file_exists(self, &new_diskq_file_stat))
        {
          msg_debug("Still waiting for new file", evt_tag_str("file", self->filename));
          return FALSE;
        }

      self->waiting_for_file_change = FALSE;
    }

  if (!_load_queue(self))
    return FALSE;

  self->diskq_file_stat = new_diskq_file_stat;
  return TRUE;
}

static void
_close_diskq(LogThreadedFetcherDriver *s)
{
  ThreadedDiskqSourceDriver *self = (ThreadedDiskqSourceDriver *) s;

  log_queue_unref(self->queue);
  self->queue = NULL;
}

/* runs in a dedicated thread */
static LogThreadedFetchResult
_fetch(LogThreadedFetcherDriver *s)
{
  ThreadedDiskqSourceDriver *self = (ThreadedDiskqSourceDriver *) s;
  LogPathOptions local_options = LOG_PATH_OPTIONS_INIT;

  gint64 remaining_messages = log_queue_get_length(self->queue);
  LogMessage *msg = log_queue_pop_head(self->queue, &local_options);

  if (!msg)
    {
      if (remaining_messages != 0)
        msg_error("Closing corrupt diskq file, waiting for new", evt_tag_long("lost_messages", remaining_messages),
                  evt_tag_str("file", self->filename));
      else
        msg_info("Diskq file has been read, waiting for new file", evt_tag_str("file", self->filename));

      _close_diskq(s);
      self->waiting_for_file_change = TRUE;

      LogThreadedFetchResult result = { THREADED_FETCH_NOT_CONNECTED, NULL };
      return result;
    }

  LogThreadedFetchResult result = { THREADED_FETCH_SUCCESS, msg };
  return result;
}

static void
_format_stats_key(LogThreadedSourceDriver *s, StatsClusterKeyBuilder *kb)
{
  ThreadedDiskqSourceDriver *self = (ThreadedDiskqSourceDriver *) s;

  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("driver", "diskq-source"));
  stats_cluster_key_builder_add_legacy_label(kb, stats_cluster_label("filename", self->filename));
}

static gboolean
_init(LogPipe *s)
{
  ThreadedDiskqSourceDriver *self = (ThreadedDiskqSourceDriver *) s;

  if (!self->filename)
    {
      msg_error("The file() option for diskq-source() is mandatory", log_pipe_location_tag(s));
      return FALSE;
    }

  stats_cluster_key_builder_free(self->queue_sck_builder);
  self->queue_sck_builder = stats_cluster_key_builder_new();

  stats_cluster_key_builder_add_label(self->queue_sck_builder,
                                      stats_cluster_label("id", self->super.super.super.super.id ? : ""));
  _format_stats_key(&self->super.super, self->queue_sck_builder);

  return log_threaded_fetcher_driver_init_method(s);
}

static void
_free(LogPipe *s)
{
  ThreadedDiskqSourceDriver *self = (ThreadedDiskqSourceDriver *) s;

  stats_cluster_key_builder_free(self->queue_sck_builder);
  g_free(self->filename);

  log_threaded_fetcher_driver_free_method(s);
}

void
threaded_diskq_sd_set_file(LogDriver *s, const gchar *filename)
{
  ThreadedDiskqSourceDriver *self = (ThreadedDiskqSourceDriver *) s;

  g_free(self->filename);
  self->filename = g_strdup(filename);
}

LogDriver *
threaded_diskq_sd_new(GlobalConfig *cfg)
{
  ThreadedDiskqSourceDriver *self = g_new0(ThreadedDiskqSourceDriver, 1);
  log_threaded_fetcher_driver_init_instance(&self->super, cfg);

  disk_queue_options_set_default_options(&self->diskq_options);

  self->queue_sck_builder = stats_cluster_key_builder_new();

  self->super.connect = _open_diskq;
  self->super.disconnect = _close_diskq;
  self->super.fetch = _fetch;

  self->super.super.super.super.super.init = _init;
  self->super.super.super.super.super.free_fn = _free;
  self->super.super.format_stats_key = _format_stats_key;

  return &self->super.super.super.super;
}
