/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#include "serialize.h"
#include "messages.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>

typedef struct _SerializeFileArchive
{
  SerializeArchive super;
  FILE *f;
} SerializeFileArchive;

typedef struct _SerializeStringArchive
{
  SerializeArchive super;
  gsize pos;
  GString *string;
} SerializeStringArchive;

typedef struct _SerializeBufferArchive
{
  SerializeArchive super;
  gsize pos;
  gsize len;
  gchar *buff;
} SerializeBufferArchive;

void
_serialize_handle_errors(SerializeArchive *self, const gchar *error_desc, GError *error)
{
  if (error)
    g_set_error(&self->error, G_FILE_ERROR, G_FILE_ERROR_IO, "%s: %s", error_desc, error->message);

  if (!self->silent)
    {
      msg_error(error_desc,
                evt_tag_str("error", error->message));
    }
  g_error_free(error);
}

void
serialize_archive_free(SerializeArchive *self)
{
  g_clear_error(&self->error);
  g_slice_free1(self->len, self);
}

static gboolean
serialize_file_archive_read_bytes(SerializeArchive *s, gchar *buf, gsize buflen, GError **error)
{
  SerializeFileArchive *self = (SerializeFileArchive *) s;
  gssize bytes_read;

  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);

  if (buflen == 0)
    {
      return TRUE;
    }

  bytes_read = fread(buf, 1, buflen, self->f);
  if (bytes_read < 0 || bytes_read != buflen)
    {
      g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_IO, "Error reading file (%s)",
                  bytes_read < 0 ? g_strerror(errno) : "short read");
      return FALSE;
    }
  return TRUE;
}

static gboolean
serialize_file_archive_write_bytes(SerializeArchive *s, const gchar *buf, gsize buflen, GError **error)
{
  SerializeFileArchive *self = (SerializeFileArchive *) s;
  gssize bytes_written;

  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);

  bytes_written = fwrite(buf, 1, buflen, self->f);
  if (bytes_written < 0 || bytes_written != buflen)
    {
      g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_IO, "Error writing file (%s)",
                  bytes_written < 0 ? g_strerror(errno) : "short write");
      return FALSE;
    }
  return TRUE;
}

SerializeArchive *
serialize_file_archive_new(FILE *f)
{
  SerializeFileArchive *self = g_slice_new0(SerializeFileArchive);

  self->super.read_bytes = serialize_file_archive_read_bytes;
  self->super.write_bytes = serialize_file_archive_write_bytes;
  self->super.len = sizeof(SerializeFileArchive);
  self->f = f;
  return &self->super;
}

void
serialize_string_archive_reset(SerializeArchive *sa)
{
  SerializeStringArchive *self = (SerializeStringArchive *) sa;

  self->pos = 0;
}

static gboolean
serialize_string_archive_read_bytes(SerializeArchive *s, gchar *buf, gsize buflen, GError **error)
{
  SerializeStringArchive *self = (SerializeStringArchive *) s;

  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);

  if (buflen == 0)
    {
      return TRUE;
    }

  if ((gssize) self->pos + buflen > (gssize) self->string->len)
    {
      g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_IO, "Error reading from string, stored data too short");
      return FALSE;
    }
  memcpy(buf, &self->string->str[self->pos], buflen);
  self->pos += buflen;
  return TRUE;
}

static gboolean
serialize_string_archive_write_bytes(SerializeArchive *s, const gchar *buf, gsize buflen, GError **error)
{
  SerializeStringArchive *self = (SerializeStringArchive *) s;

  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);

  g_string_append_len(self->string, buf, buflen);
  return TRUE;
}

SerializeArchive *
serialize_string_archive_new(GString *str)
{
  SerializeStringArchive *self = g_slice_new0(SerializeStringArchive);

  self->super.read_bytes = serialize_string_archive_read_bytes;
  self->super.write_bytes = serialize_string_archive_write_bytes;
  self->super.len = sizeof(SerializeStringArchive);
  self->string = str;
  return &self->super;
}

static gboolean
serialize_buffer_archive_read_bytes(SerializeArchive *s, gchar *buf, gsize buflen, GError **error)
{
  SerializeBufferArchive *self = (SerializeBufferArchive *) s;

  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);

  if ((gssize) self->pos + buflen > (gssize) self->len)
    {
      g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_IO, "Error reading from buffer, stored data too short");
      return FALSE;
    }
  memcpy(buf, &self->buff[self->pos], buflen);
  self->pos += buflen;
  return TRUE;
}

static gboolean
serialize_buffer_archive_write_bytes(SerializeArchive *s, const gchar *buf, gsize buflen, GError **error)
{
  SerializeBufferArchive *self = (SerializeBufferArchive *) s;

  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);

  if (self->pos + buflen > self->len)
    {
      g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_IO, "Error writing to buffer, buffer is too small");
      return FALSE;
    }

  memcpy(self->buff + self->pos, buf, buflen);
  self->pos += buflen;
  return TRUE;
}

gsize
serialize_buffer_archive_get_pos(SerializeArchive *s)
{
  SerializeBufferArchive *self = (SerializeBufferArchive *) s;

  return self->pos;
}

SerializeArchive *
serialize_buffer_archive_new(gchar *buff, gsize len)
{
  SerializeBufferArchive *self = g_slice_new0(SerializeBufferArchive);

  self->super.read_bytes = serialize_buffer_archive_read_bytes;
  self->super.write_bytes = serialize_buffer_archive_write_bytes;
  self->super.len = sizeof(SerializeBufferArchive);
  self->buff = buff;
  self->len = len;
  return &self->super;
}
