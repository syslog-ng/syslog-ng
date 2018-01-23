/*
 * Copyright (c) 2002-2010 Balabit
 * Copyright (c) 1998-2010 Balázs Scheidler
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

#ifndef SERIALIZE_H_INCLUDED
#define SERIALIZE_H_INCLUDED

#include "syslog-ng.h"

#include <stdio.h>
#include <string.h>

typedef struct _SerializeArchive SerializeArchive;

struct _SerializeArchive
{
  GError *error;
  guint16 len;
  guint16 silent:1;

  gboolean (*read_bytes)(SerializeArchive *archive, gchar *buf, gsize count, GError **error);
  gboolean (*write_bytes)(SerializeArchive *archive, const gchar *buf, gsize count, GError **error);
};

/* this is private and is only published so that the inline functions below can invoke it */
void _serialize_handle_errors(SerializeArchive *self, const gchar *error_desc, GError *error);

static inline gboolean
serialize_archive_read_bytes(SerializeArchive *self, gchar *buf, gsize buflen)
{
  GError *error = NULL;

  if ((self->error == NULL) && !self->read_bytes(self, buf, buflen, &error))
    _serialize_handle_errors(self, "Error reading serialized data", error);
  return self->error == NULL;
}

static inline gboolean
serialize_archive_write_bytes(SerializeArchive *self, const gchar *buf, gsize buflen)
{
  GError *error = NULL;

  if ((self->error == NULL) && !self->write_bytes(self, buf, buflen, &error))
    _serialize_handle_errors(self, "Error writing serialized data", error);
  return self->error == NULL;
}

static inline gboolean
serialize_write_uint32(SerializeArchive *archive, guint32 value)
{
  guint32 n;

  n = GUINT32_TO_BE(value);
  return serialize_archive_write_bytes(archive, (gchar *) &n, sizeof(n));
}

static inline gboolean
serialize_read_uint32(SerializeArchive *archive, guint32 *value)
{
  guint32 n;

  if (serialize_archive_read_bytes(archive, (gchar *) &n, sizeof(n)))
    {
      *value = GUINT32_FROM_BE(n);
      return TRUE;
    }
  return FALSE;
}

/* NOTE: this function writes to the array to convert it to big endian. It
 * is converted back to native byte order before returning */
static inline gboolean
serialize_write_uint32_array(SerializeArchive *archive, guint32 *values, gsize elements)
{
  const int buffer_size = 128;
  guint32 converted_values[buffer_size];
  gint converted_ndx;

  while (elements > 0)
    {
      for (converted_ndx = 0;
           converted_ndx < buffer_size && converted_ndx < elements;
           converted_ndx++)
        converted_values[converted_ndx] = GUINT32_TO_BE(values[converted_ndx]);

      if (!serialize_archive_write_bytes(archive, (void *) converted_values, converted_ndx * sizeof(guint32)))
        return FALSE;

      values += converted_ndx;
      elements -= converted_ndx;
    }
  return TRUE;
}

static inline gboolean
serialize_read_uint32_array(SerializeArchive *archive, guint32 *values, gsize elements)
{
  if (serialize_archive_read_bytes(archive, (void *) values, elements * sizeof(guint32)))
    {
      for (int i = 0; i < elements; i++)
        values[i] = GUINT32_FROM_BE(values[i]);
      return TRUE;
    }
  return FALSE;
}

static inline gboolean
serialize_read_uint16_array(SerializeArchive *archive, guint32 *values, gsize elements)
{
  guint16 buffer[elements];

  if (serialize_archive_read_bytes(archive, (void *) &buffer, elements * sizeof(guint16)))
    {
      for (int i = 0; i < elements; i++)
        values[i] = GUINT16_FROM_BE(buffer[i]);
      return TRUE;
    }
  return FALSE;
}

static inline gboolean
serialize_write_uint64(SerializeArchive *archive, guint64 value)
{
  guint64 n;

  n = GUINT64_TO_BE(value);
  return serialize_archive_write_bytes(archive, (gchar *) &n, sizeof(n));
}

static inline gboolean
serialize_read_uint64(SerializeArchive *archive, guint64 *value)
{
  guint64 n;

  if (serialize_archive_read_bytes(archive, (gchar *) &n, sizeof(n)))
    {
      *value = GUINT64_FROM_BE(n);
      return TRUE;
    }
  return FALSE;
}

static inline gboolean
serialize_write_uint16(SerializeArchive *archive, guint16 value)
{
  guint16 n;

  n = GUINT16_TO_BE(value);
  return serialize_archive_write_bytes(archive, (gchar *) &n, sizeof(n));
}

static inline gboolean
serialize_read_uint16(SerializeArchive *archive, guint16 *value)
{
  guint16 n;

  if (serialize_archive_read_bytes(archive, (gchar *) &n, sizeof(n)))
    {
      *value = GUINT16_FROM_BE(n);
      return TRUE;
    }
  return FALSE;
}

static inline gboolean
serialize_write_uint8(SerializeArchive *archive, guint8 value)
{
  guint8 n;

  n = value;
  return serialize_archive_write_bytes(archive, (gchar *) &n, sizeof(n));
}

static inline gboolean
serialize_read_uint8(SerializeArchive *archive, guint8 *value)
{
  guint8 n;

  if (serialize_archive_read_bytes(archive, (gchar *) &n, sizeof(n)))
    {
      *value = n;
      return TRUE;
    }
  return FALSE;
}


static inline gboolean
serialize_write_blob(SerializeArchive *archive, const void *blob, gsize len)
{
  return serialize_archive_write_bytes(archive, blob, len);
}

static inline gboolean
serialize_read_blob(SerializeArchive *archive, void *blob, gsize len)
{
  return serialize_archive_read_bytes(archive, blob, len);
}

static inline gboolean
serialize_write_string(SerializeArchive *archive, GString *str)
{
  return serialize_write_uint32(archive, str->len) &&
         serialize_archive_write_bytes(archive, str->str, str->len);
}

static inline gboolean
serialize_read_string(SerializeArchive *archive, GString *str)
{
  guint32 len;

  if (serialize_read_uint32(archive, &len))
    {
      if (len > str->allocated_len)
        {
          gchar *p;

          p = g_try_realloc(str->str, len + 1);
          if (!p)
            return FALSE;
          str->str = p;
          str->str[len] = 0;
          str->len = len;
        }
      else
        g_string_set_size(str, len);

      return serialize_archive_read_bytes(archive, str->str, len);
    }
  return FALSE;
}

static inline gboolean
serialize_write_cstring(SerializeArchive *archive, const gchar *str, gssize len)
{
  if (len < 0)
    len = strlen(str);

  return serialize_write_uint32(archive, len) &&
         (len == 0 || serialize_archive_write_bytes(archive, str, len));
}

static inline gboolean
serialize_read_cstring(SerializeArchive *archive, gchar **str, gsize *str_len)
{
  guint32 len;

  if (serialize_read_uint32(archive, &len))
    {
      *str = g_try_malloc(len + 1);

      if (!(*str))
        return FALSE;
      (*str)[len] = 0;
      if (str_len)
        *str_len = len;
      return serialize_archive_read_bytes(archive, *str, len);
    }
  return FALSE;
}


SerializeArchive *serialize_file_archive_new(FILE *f);
SerializeArchive *serialize_string_archive_new(GString *str);
void serialize_string_archive_reset(SerializeArchive *sa);
SerializeArchive *serialize_buffer_archive_new(gchar *buff, gsize len);
gsize serialize_buffer_archive_get_pos(SerializeArchive *self);
void serialize_archive_free(SerializeArchive *self);

#endif
