/*
 * Copyright (c) 2025 Airbus Commercial Aircraft
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "slog_file.h"

// File access modes
static const char modes[NUM_MODES][LEN_MODES] = { "r", "r+", "w", "w+", "a", "a+" };

static gboolean close_channel(SLogFile *f);

SLogFile *create_file(const gchar *filename, const gchar *mode)
{
  SLogFile *f = g_new0(SLogFile, 1);

  if (f == NULL)
    {
      return NULL;
    }

  f->filename = filename;
  f->channel = NULL;
  f->mode = mode;
  f->status = G_IO_STATUS_NORMAL;
  f->error = NULL;
  f->message = g_string_new(SLOG_INFO_PREFIX);
  f->state = SLOG_FILE_READY;
  return f;
}

gboolean open_file(SLogFile *f)
{
  if (f == NULL || f->state != SLOG_FILE_READY)
    {
      return FALSE;
    }

  gboolean result = TRUE;
  gboolean found = FALSE;
  for (int i = 0; i < NUM_MODES; i++)
    {
      if (strncmp(modes[i], f->mode, LEN_MODES) == 0)
        {
          found = TRUE;
          break;
        }
    }

  if (!found)
    {
      g_set_error (&f->error,
                   SLOG_FILE_DOMAIN,          // Error domain
                   SLOG_FILE_INVALID_MODE,    // Error code
                   "Invalid mode: %s",        // Error message format string
                   f->mode);
      f->status = G_IO_STATUS_ERROR;
      f->state = SLOG_FILE_INVALID_MODE;
    }
  else
    {
      f->channel = g_io_channel_new_file(f->filename, f->mode, &f->error);
      if (!f->channel)
        {
          f->status = G_IO_STATUS_ERROR;
          f->state = SLOG_FILE_GENERAL_ERROR;
        }
      else
        {
          f->status = g_io_channel_set_encoding(f->channel, NULL, &f->error); // NULL means encoding for binary data
        }
    }

  // Assemble status message
  if (f->status != G_IO_STATUS_NORMAL)
    {
      switch (f->status)
        {
        case G_IO_STATUS_ERROR:
          g_string_assign(f->message, "G_IO_STATUS_ERROR -");
          f->state = SLOG_FILE_GENERAL_ERROR;
          break;
        case G_IO_STATUS_EOF:
          g_string_assign(f->message, "G_IO_STATUS_EOF -");
          f->state = SLOG_FILE_EOF;
          break;
        case G_IO_STATUS_AGAIN:
          g_string_assign(f->message, "G_IO_STATUS_AGAIN -");
          f->state = SLOG_FILE_RESOURCE_UNAVAILABLE;
          break;
        default:
          break;
        }
      result = FALSE;
    }
  else
    {
      g_string_append_printf(f->message, "File: %s, Mode: %s, Status: G_IO_STATUS_NORMAL (%d)",
                             f->filename, f->mode, f->status);
      f->state = SLOG_FILE_OPEN;
    }

  if (f->error != NULL)
    {
      g_string_append_printf(f->message, " %s %s", f->error->message, f->filename);
      g_clear_error(&f->error);
    }

  return result;
}


gboolean write_to_file(SLogFile *f, const gchar *data, gsize len)
{
  // File must be open
  if (f == NULL || f->state != SLOG_FILE_OPEN)
    {
      return FALSE;
    }
  gsize chars_written = 0;
  f->status = g_io_channel_write_chars(f->channel, data, len, &chars_written, &f->error);

  gboolean result = f->status == G_IO_STATUS_NORMAL;

  if (chars_written != len)
    {
      g_string_assign(f->message, SLOG_ERROR_PREFIX);
      g_string_append_printf(f->message,
                             "File: %s, Mode: %s, Only %zu bytes written. Expected %zu bytes",
                             f->filename,
                             f->mode,
                             chars_written,
                             len);
      result = FALSE;
      f->state = SLOG_FILE_INCOMPLETE_WRITE;
    }

  return result;
}

gboolean read_from_file(SLogFile *f, gchar *data, gsize len)
{
  // File must be open
  if (f == NULL || f->state != SLOG_FILE_OPEN)
    {
      return FALSE;
    }
  gsize chars_read = 0;
  f->status = g_io_channel_read_chars(f->channel, data, len, &chars_read, &f->error);

  gboolean result = f->status == G_IO_STATUS_NORMAL;

  if (chars_read != len)
    {
      g_string_assign(f->message, SLOG_ERROR_PREFIX);
      g_string_append_printf(f->message,
                             "File: %s, Mode: %s -> Only %zu bytes read. Expected %zu bytes",
                             f->filename,
                             f->mode,
                             chars_read,
                             len);
      result = FALSE;
      f->state = SLOG_FILE_INCOMPLETE_READ;
    }

  return result;
}

gboolean read_line_from_file(SLogFile *f, GString *line)
{
  // File must be open
  if (f == NULL || f->state != SLOG_FILE_OPEN)
    {
      return FALSE;
    }

  f->status =  g_io_channel_read_line_string(f->channel, line, NULL, &f->error);

  gboolean result = f->status == G_IO_STATUS_NORMAL;

  return result;
}

gboolean close_channel(SLogFile *f)
{
  if (f == NULL || f->channel == NULL)
    {
      f->state = SLOG_FILE_GENERAL_ERROR;
      return FALSE;
    }

  // TRUE means flush the channel
  f->status = g_io_channel_shutdown(f->channel, TRUE, &f->error);
  g_io_channel_unref(f->channel);

  f->channel = NULL;

  if (f->status != G_IO_STATUS_NORMAL)
    {
      f->state = SLOG_FILE_SHUTDOWN_ERROR;
      return FALSE;
    }
  else
    {
      f->state = SLOG_FILE_CLOSED;
      return TRUE;
    }
}

gboolean close_file(SLogFile *f)
{
  if (f == NULL)
    {
      return FALSE;
    }
  gboolean result = close_channel(f);

  // Release resources
  g_string_free(f->message, TRUE);

  return result;
}
