/*
 * Copyright (c) 2017 Balabit
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

#include "wildcard-source.h"
#include "directory-monitor-factory.h"
#include "messages.h"
#include "file-specializations.h"
#include "mainloop.h"

#include <fcntl.h>

#define DEFAULT_SD_OPEN_FLAGS (O_RDONLY | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)

static DirectoryMonitor *_add_directory_monitor(WildcardSourceDriver *self, const gchar *directory);


static gboolean
_check_required_options(WildcardSourceDriver *self)
{
  if (!self->base_dir)
    {
      msg_error("Error: base-dir option is required",
                evt_tag_str("driver", self->super.super.id));
      return FALSE;
    }
  if (!self->filename_pattern)
    {
      msg_error("Error: filename-pattern option is required",
                evt_tag_str("driver", self->super.super.id));
      return FALSE;
    }
  return TRUE;
}

static void
_deleted_cb(FileReader *self, gpointer user_data)
{
  log_pipe_deinit(&self->super);
  file_reader_remove_persist_state(self);
}

static void
_stop_file_reader(FileStateHandler *state_handler, gpointer user_data)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *) user_data;
  FileReader *reader = g_hash_table_lookup(self->file_readers, state_handler->filename);
  if (!reader)
    {
      msg_error("File reader isn't found for file",
                evt_tag_str("filename", state_handler->filename));
    }
  else
    {
      msg_debug("Stop following file, because of deleted and eof",
                evt_tag_str("filename", state_handler->filename));
      file_reader_stop_follow_file(reader);
    }
}

void
_create_file_reader(WildcardSourceDriver *self, const gchar *full_path)
{
  FileReader *reader = NULL;
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  if (g_hash_table_size(self->file_readers) >= self->max_files)
    {
      msg_warning("Number of allowed monitorod file is reached, rejecting read file",
                  evt_tag_str("source", self->super.super.group),
                  evt_tag_str("filename", full_path),
                  evt_tag_int("max_files", self->max_files));
      file_list_add(self->waiting_list, full_path);
      return;
    }

  gchar *filename = g_strdup(full_path);

  FileStateHandler *state_handler = file_state_handler_new(filename, &self->deleted_file_events);

  reader = file_reader_new(full_path,
                           &self->file_reader_options,
                           self->file_opener,
                           &self->super,
                           cfg,
                           state_handler);

  log_pipe_append(&reader->super, &self->super.super.super);
  if (!log_pipe_init(&reader->super))
    {
      msg_warning("File reader initialization failed",
                  evt_tag_str("filename", full_path),
                  evt_tag_str("source_driver", self->super.super.group));
      log_pipe_unref(&reader->super);
      g_free(filename);
    }
  else
    {
      g_hash_table_insert(self->file_readers, filename, reader);
    }
}

static void
_handle_file_created(WildcardSourceDriver *self, const DirectoryMonitorEvent *event)
{
  if (g_pattern_match_string(self->compiled_pattern, event->name))
    {
      FileReader *reader = g_hash_table_lookup(self->file_readers, event->full_path);

      if (!reader)
        {
          _create_file_reader(self, event->full_path);
          msg_debug("Wildcard: file created", evt_tag_str("filename", event->full_path));
        }
      else
        {
          if (reader->file_state->deleted)
            {
              msg_info("File is deleted, new file create with same name. "
                       "While old file is reading, skip the new one",
                       evt_tag_str("filename", event->full_path));
              file_list_add(self->waiting_list, event->full_path);
            }
          else if (!log_pipe_init(&reader->super))
            {
              msg_error("Can not re-initialize reader for file",
                        evt_tag_str("filename", event->full_path));
            }
          else
            {
              msg_debug("Wildcard: file reader reinitialized", evt_tag_str("filename", event->full_path));
            }
        }
    }
}

void
_handle_directory_created(WildcardSourceDriver *self, const DirectoryMonitorEvent *event)
{
  if (self->recursive)
    {
      msg_debug("Directory created",
                evt_tag_str("name", event->full_path));
      DirectoryMonitor *monitor = g_hash_table_lookup(self->directory_monitors, event->full_path);
      if (!monitor)
        {
          _add_directory_monitor(self, event->full_path);
        }
    }
}

void
_handle_deleted(WildcardSourceDriver *self, const DirectoryMonitorEvent *event)
{
  FileReader *reader = g_hash_table_lookup(self->file_readers, event->full_path);

  if (reader)
    {
      msg_debug("Monitored file is deleted", evt_tag_str("filename", event->full_path));
      log_pipe_notify(&reader->super, NC_FILE_DELETED, NULL);
    }
  else if (g_hash_table_remove(self->directory_monitors, event->full_path))
    {
      msg_debug("Monitored directory is deleted", evt_tag_str("directory", event->full_path));
    }
  else
    {
      if (file_list_remove(self->waiting_list, event->full_path))
        {
          msg_warning("Waiting file was deleted, it wasn't read at all", evt_tag_str("filename", event->full_path));
        }
    }
}

static void
_on_directory_monitor_changed(const DirectoryMonitorEvent *event, gpointer user_data)
{
  main_loop_assert_main_thread();

  WildcardSourceDriver *self = (WildcardSourceDriver *)user_data;

  if ((event->event_type == FILE_CREATED))
    {
      _handle_file_created(self, event);
    }
  else if (event->event_type == DIRECTORY_CREATED)
    {
      _handle_directory_created(self, event);
    }
  else if (event->event_type == DELETED)
    {
      _handle_deleted(self, event);
    }
}

static void
_remove_file_reader(FileStateHandler *state_handler, gpointer user_data)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *) user_data;
  FileReader *reader = g_hash_table_lookup(self->file_readers, state_handler->filename);
  if (!reader)
    {
      msg_error("File reader isn't found for file",
                evt_tag_str("filename", state_handler->filename));
      return;
    }
  _deleted_cb(reader, user_data);
  log_pipe_ref(&reader->super);
  if (g_hash_table_remove(self->file_readers, reader->filename->str))
    {
      msg_debug("File is removed from the file list", evt_tag_str("Filename", reader->filename->str));
    }
  else
    {
      msg_error("Can't remove the file reader", evt_tag_str("Filename", reader->filename->str));
    }
  log_pipe_unref(&reader->super);
  gchar *full_path = file_list_pop(self->waiting_list);
  if (full_path)
    {
      _create_file_reader(self, full_path);
      g_free(full_path);
    }
}


static void
_ensure_minimum_window_size(WildcardSourceDriver *self)
{
  if (self->file_reader_options.reader_options.super.init_window_size < MINIMUM_WINDOW_SIZE)
    {
      msg_warning("log_iw_size configuration value was divided by the value of max-files()."
                  " The result was too small, clamping to minimum entries."
                  " Ensure you have a proper log_fifo_size setting to avoid message loss.",
                  evt_tag_int("orig_log_iw_size", self->file_reader_options.reader_options.super.init_window_size),
                  evt_tag_int("new_log_iw_size", MINIMUM_WINDOW_SIZE),
                  evt_tag_int("min_log_fifo_size", MINIMUM_WINDOW_SIZE * self->max_files));
      self->file_reader_options.reader_options.super.init_window_size = MINIMUM_WINDOW_SIZE;
    }
}

static void
_init_reader_options(WildcardSourceDriver *self, GlobalConfig *cfg)
{
  if (!self->window_size_initialized)
    {
      self->file_reader_options.reader_options.super.init_window_size /= self->max_files;
      _ensure_minimum_window_size(self);
      self->window_size_initialized = TRUE;

    }
  file_reader_options_init(&self->file_reader_options, cfg, self->super.super.group);
}

static void
_init_opener_options(WildcardSourceDriver *self, GlobalConfig *cfg)
{
  file_opener_options_init(&self->file_opener_options, cfg);
  file_opener_set_options(self->file_opener, &self->file_opener_options);
}

static gboolean
_init_filename_pattern(WildcardSourceDriver *self)
{
  self->compiled_pattern = g_pattern_spec_new(self->filename_pattern);
  if (!self->compiled_pattern)
    {
      msg_error("Invalid filename-pattern",
                evt_tag_str("filename-pattern", self->filename_pattern));
      return FALSE;
    }
  return TRUE;
}

static DirectoryMonitor *
_add_directory_monitor(WildcardSourceDriver *self, const gchar *directory)
{
  DirectoryMonitorOptions options =
  {
    .dir = directory,
    .follow_freq = self->file_reader_options.follow_freq,
    .method = self->monitor_method
  };
  DirectoryMonitor *monitor = create_directory_monitor(&options);
  if (!monitor)
    {
      msg_error("Wildcard source: could not create directory monitoring object! Possible message loss",
                evt_tag_str("dir", directory),
                log_pipe_location_tag(&self->super.super.super));
      return NULL;
    }

  directory_monitor_set_callback(monitor, _on_directory_monitor_changed, self);
  directory_monitor_start(monitor);
  g_hash_table_insert(self->directory_monitors, g_strdup(directory), monitor);
  return monitor;
}

static gboolean
_init(LogPipe *s)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_src_driver_init_method(s))
    {
      return FALSE;
    }
  if (!_check_required_options(self))
    {
      return FALSE;
    }

  if (!_init_filename_pattern(self))
    {
      return FALSE;
    }

  _init_reader_options(self, cfg);
  _init_opener_options(self, cfg);

  if (!_add_directory_monitor(self, self->base_dir))
    return FALSE;

  return TRUE;
}

static void
_deinit_reader(gpointer key, gpointer value, gpointer user_data)
{
  FileReader *reader = (FileReader *) value;

  log_pipe_deinit(&reader->super);
}

static gboolean
_deinit(LogPipe *s)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)s;

  g_pattern_spec_free(self->compiled_pattern);
  g_hash_table_foreach(self->file_readers, _deinit_reader, NULL);
  return TRUE;
}

void
wildcard_sd_set_base_dir(LogDriver *s, const gchar *base_dir)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)s;

  g_free(self->base_dir);
  self->base_dir = g_strdup(base_dir);
}

void
wildcard_sd_set_filename_pattern(LogDriver *s, const gchar *filename_pattern)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)s;

  g_free(self->filename_pattern);
  self->filename_pattern = g_strdup(filename_pattern);
}

void
wildcard_sd_set_recursive(LogDriver *s, gboolean recursive)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)s;

  self->recursive = recursive;
}

gboolean
wildcard_sd_set_monitor_method(LogDriver *s, const gchar *method)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)s;
  MonitorMethod new_method = directory_monitor_factory_get_monitor_method(method);

  if (new_method == MM_UNKNOWN)
    {
      msg_error("Invalid monitor-method",
                evt_tag_str("monitor-method", method));
      return FALSE;
    }
  self->monitor_method = new_method;
  return TRUE;
}

void
wildcard_sd_set_max_files(LogDriver *s, guint32 max_files)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)s;

  self->max_files = max_files;
}

static void
_free(LogPipe *s)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)s;

  file_opener_free(self->file_opener);
  g_free(self->base_dir);
  g_free(self->filename_pattern);
  g_hash_table_unref(self->file_readers);
  g_hash_table_unref(self->directory_monitors);
  file_reader_options_deinit(&self->file_reader_options);
  file_opener_options_deinit(&self->file_opener_options);
  file_list_free(self->waiting_list);
  log_src_driver_free(s);
}

static void
_stop_and_destroy_directory_monitor(gpointer s)
{
  DirectoryMonitor *monitor = (DirectoryMonitor *)s;

  directory_monitor_stop(monitor);
  directory_monitor_free(monitor);
}

LogDriver *
wildcard_sd_new(GlobalConfig *cfg)
{
  WildcardSourceDriver *self = g_new0(WildcardSourceDriver, 1);

  log_src_driver_init_instance(&self->super, cfg);

  self->super.super.super.free_fn = _free;
  self->super.super.super.init = _init;
  self->super.super.super.deinit = _deinit;

  self->file_readers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)log_pipe_unref);
  self->directory_monitors = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                                   (GDestroyNotify)_stop_and_destroy_directory_monitor);

  self->monitor_method = MM_AUTO;

  file_reader_options_defaults(&self->file_reader_options);
  file_opener_options_defaults_dont_change_permissions(&self->file_opener_options);
  self->file_reader_options.follow_freq = 1000;
  self->file_reader_options.reader_options.super.init_window_size = MINIMUM_WINDOW_SIZE * DEFAULT_MAX_FILES;
  self->file_reader_options.reader_options.super.stats_source = SCS_FILE;
  self->file_reader_options.restore_state = TRUE;

  self->max_files = DEFAULT_MAX_FILES;
  self->file_opener = file_opener_for_regular_source_files_new();

  self->deleted_file_events.user_data = self;
  self->deleted_file_events.deleted_file_eof = _stop_file_reader;
  self->deleted_file_events.deleted_file_finised = _remove_file_reader;

  self->waiting_list = file_list_new();

  return &self->super.super;
}
