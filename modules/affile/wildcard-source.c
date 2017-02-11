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
_free(LogPipe *s)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)s;
  g_string_free(self->base_dir, TRUE);
  g_string_free(self->filename_pattern, TRUE);
  g_hash_table_unref(self->file_readers);
  file_reader_options_destroy(&self->file_reader_options);
  log_src_driver_free(s);
}

static gboolean
_init(LogPipe *s)
{
  WildcardSourceDriver *self = (WildcardSourceDriver *)s;
  if (!log_src_driver_init_method(s))
    {
      return FALSE;
    }
  if (!_check_required_options(self))
    {
      return FALSE;
    }
  return TRUE;
}

LogDriver *
wildcard_sd_new(GlobalConfig *cfg)
{
  WildcardSourceDriver *self = g_new0(WildcardSourceDriver, 1);

  log_src_driver_init_instance(&self->super, cfg);

  self->super.super.super.free_fn = _free;
  self->super.super.super.init = _init;

  return &self->super.super;
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
