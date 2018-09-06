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
      self->diskq_options.disk_buf_size = 128;
      self->diskq_options.mem_buf_size = 1024 * 1024;
      self->queue = log_queue_disk_reliable_new(&self->diskq_options, NULL);
    }
  else
    {
      self->diskq_options.disk_buf_size = 1;
      self->diskq_options.mem_buf_size = 128;
      self->diskq_options.qout_size = 128;
      self->queue = log_queue_disk_non_reliable_new(&self->diskq_options, NULL);
    }

  if (!log_queue_disk_load_queue(self->queue, self->filename))
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

static gboolean
_init(LogPipe *s)
{
  ThreadedDiskqSourceDriver *self = (ThreadedDiskqSourceDriver *) s;

  if (!self->filename)
    {
      msg_error("The file() option for diskq-source() is mandatory", log_pipe_location_tag(s));
      return FALSE;
    }

  return log_threaded_fetcher_driver_init_method(s);
}

static void
_free(LogPipe *s)
{
  ThreadedDiskqSourceDriver *self = (ThreadedDiskqSourceDriver *) s;

  g_free(self->filename);

  log_threaded_fetcher_driver_free_method(s);
}

static const gchar *
_format_stats_instance(LogThreadedSourceDriver *s)
{
  ThreadedDiskqSourceDriver *self = (ThreadedDiskqSourceDriver *) s;
  static gchar persist_name[1024];

  if (s->super.super.super.persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "diskq-source,%s", s->super.super.super.persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "diskq-source,%s", self->filename);

  return persist_name;
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

  self->super.connect = _open_diskq;
  self->super.disconnect = _close_diskq;
  self->super.fetch = _fetch;

  self->super.super.super.super.super.init = _init;
  self->super.super.super.super.super.free_fn = _free;
  self->super.super.format_stats_instance = _format_stats_instance;

  return &self->super.super.super.super;
}
