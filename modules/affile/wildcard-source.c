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
#include "messages.h"
#include <fcntl.h>

#define DEFAULT_SD_OPEN_FLAGS (O_RDONLY | O_NOCTTY | O_NONBLOCK | O_LARGEFILE)


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

void
_create_file_reader (WildcardSourceDriver* self, const gchar* name, const gchar* full_path)
{
  FileReader* reader = NULL;
  if (g_hash_table_size (self->file_readers) >= self->max_files)
    {
      msg_warning("Number of allowed monitorod file is reached, rejecting read file",
                  evt_tag_str ("source", self->super.super.group), evt_tag_str ("filename", name),
                  evt_tag_int ("max_files", self->max_files));
    }
  GlobalConfig* cfg = log_pipe_get_config (&self->super.super.super);
  reader = file_reader_new (full_path, &self->super, cfg);
  reader->file_reader_options = &self->file_reader_options;
  log_pipe_append (&reader->super, &self->super.super.super);
  if (!log_pipe_init (&reader->super))
    {
      msg_warning("File reader initialization failed", evt_tag_str ("filename", name),
                  evt_tag_str ("source_driver", self->super.super.group));
      log_pipe_unref (&reader->super);
    }
  else
    {
      g_hash_table_insert (self->file_readers, g_strdup (full_path), reader);
    }
}

static void
_start_file_reader(const DirectoryMonitorEvent *event, gpointer user_data)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)user_data;
  if ((event->file_type == FILE_IS_REGULAR) && (g_pattern_match_string(self->compiled_pattern, event->name)))
    {
      FileReader *reader = g_hash_table_lookup(self->file_readers, event->full_path);

      if (!reader)
        {
          _create_file_reader (self, event->name, event->full_path);
        }
    }
  else
    {
      msg_debug("Filename does not match with pattern",
                evt_tag_str("filename", event->name),
                evt_tag_str("full_path", event->full_path),
                evt_tag_str("pattern", self->filename_pattern->str));
    }
}

static void
_ensure_minimum_window_size (WildcardSourceDriver* self)
{
  if (self->file_reader_options.reader_options.super.init_window_size < MINIMUM_WINDOW_SIZE)
    {
      msg_warning("log_iw_size configuration value was divided by the value of max-files()."
                  " The result was too small, clamping to minimum entries."
                  " Ensure you have a proper log_fifo_size setting to avoid message loss.",
                  evt_tag_int ("orig_log_iw_size", self->file_reader_options.reader_options.super.init_window_size),
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
      _ensure_minimum_window_size (self);
      self->window_size_initialized = TRUE;

    }
  log_reader_options_init(&self->file_reader_options.reader_options, cfg, self->super.super.group);
}

static gboolean
_init_filename_pattern(WildcardSourceDriver *self)
{
  self->compiled_pattern = g_pattern_spec_new(self->filename_pattern->str);
  if (!self->compiled_pattern)
    {
      msg_error("Invalid filename-pattern",
                evt_tag_str("filename-pattern", self->filename_pattern->str));
      return FALSE;
    }
  return TRUE;
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
  self->directory_monitor = directory_monitor_new(self->base_dir->str);
  directory_monitor_set_callback(self->directory_monitor, _start_file_reader, self);
  directory_monitor_start(self->directory_monitor);
  return TRUE;
}

static void
_deinit_reader(gpointer key, gpointer value, gpointer user_data)
{
  FileReader *reader = (FileReader *)value;
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
  if (!self->base_dir)
    {
      self->base_dir = g_string_new(base_dir);
    }
  else
    {
      g_string_assign(self->base_dir, base_dir);
    }
}

void
wildcard_sd_set_filename_pattern(LogDriver *s, const gchar *filename_pattern)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)s;
  if (!self->filename_pattern)
    {
      self->filename_pattern = g_string_new(filename_pattern);
    }
  else
    {
      g_string_assign(self->filename_pattern, filename_pattern);
    }
}

void
wildcard_sd_set_recursive(LogDriver *s, gboolean recursive)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)s;
  self->recursive = recursive;
}

void
wildcard_sd_set_force_directory_polling(LogDriver *s, gboolean force_directory_polling)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)s;
  self->force_dir_polling = force_directory_polling;
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
  g_string_free(self->base_dir, TRUE);
  g_string_free(self->filename_pattern, TRUE);
  g_hash_table_unref(self->file_readers);
  file_reader_options_destroy(&self->file_reader_options);
  directory_monitor_free(self->directory_monitor);
  log_src_driver_free(s);
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
  log_reader_options_defaults(&self->file_reader_options.reader_options);
  file_perm_options_defaults(&self->file_reader_options.file_perm_options);

  self->file_reader_options.reader_options.parse_options.flags |= LP_LOCAL;
  self->file_reader_options.file_open_options.is_pipe = FALSE;
  self->file_reader_options.file_open_options.open_flags = DEFAULT_SD_OPEN_FLAGS;
  self->file_reader_options.follow_freq = 1000;

  self->max_files = DEFAULT_MAX_FILES;
  self->file_reader_options.reader_options.super.init_window_size = MINIMUM_WINDOW_SIZE * self->max_files;

  return &self->super.super;
}
