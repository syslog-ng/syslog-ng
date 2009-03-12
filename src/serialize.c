/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "serialize.h"
#include "messages.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>

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

static gboolean
serialize_archive_read_bytes(SerializeArchive *self, gchar *buf, gsize buflen)
{
  if ((self->error == NULL) && !self->read_bytes(self, buf, buflen, &self->error))
    {
      if (!self->error)
        {
          g_set_error(&self->error, G_FILE_ERROR, G_FILE_ERROR_IO, "Error reading data");
        }
      msg_error("Error reading serialized data",
                evt_tag_str("error", self->error->message),
                NULL);
    }
  return self->error == NULL;
}

static gboolean
serialize_archive_write_bytes(SerializeArchive *self, const gchar *buf, gsize buflen)
{
  if ((self->error == NULL) && !self->write_bytes(self, buf, buflen, &self->error))
    {
      if (!self->error)
        {
          g_set_error(&self->error, G_FILE_ERROR, G_FILE_ERROR_IO, "Error writing data");
        }
      msg_error("Error writing serializing data",
                evt_tag_str("error", self->error->message),
                NULL);
    }
  return self->error == NULL;
}


void
serialize_archive_free(SerializeArchive *self)
{
  g_free(self);
}

static gboolean 
serialize_file_archive_read_bytes(SerializeArchive *s, gchar *buf, gsize buflen, GError **error)
{
  SerializeFileArchive *self = (SerializeFileArchive *) s;
  gsize bytes_read;
  
  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);
  
  bytes_read = fread(buf, 1, buflen, self->f);
  if (bytes_read < 0 || bytes_read != buflen)
    {
      g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_IO, "Error reading file (%s)", bytes_read < 0 ? g_strerror(errno) : "short read");
      return FALSE;
    }
  return TRUE;
}

static gboolean 
serialize_file_archive_write_bytes(SerializeArchive *s, const gchar *buf, gsize buflen, GError **error)
{
  SerializeFileArchive *self = (SerializeFileArchive *) s;
  gsize bytes_written;
  
  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);
  
  bytes_written = fwrite(buf, 1, buflen, self->f);
  if (bytes_written < 0 || bytes_written != buflen)
    {
      g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_IO, "Error writing file (%s)", bytes_written < 0 ? g_strerror(errno) : "short write");
      return FALSE;
    }
  return TRUE;
}

SerializeArchive *
serialize_file_archive_new(FILE *f)
{
  SerializeFileArchive *self = g_new0(SerializeFileArchive, 1);
  
  self->super.read_bytes = serialize_file_archive_read_bytes;
  self->super.write_bytes = serialize_file_archive_write_bytes;
  self->f = f;
  return &self->super;
}

static gboolean 
serialize_string_archive_read_bytes(SerializeArchive *s, gchar *buf, gsize buflen, GError **error)
{
  SerializeStringArchive *self = (SerializeStringArchive *) s;
  
  g_return_val_if_fail(error == NULL || (*error) == NULL, FALSE);
  
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
  SerializeStringArchive *self = g_new0(SerializeStringArchive, 1);
  
  self->super.read_bytes = serialize_string_archive_read_bytes;
  self->super.write_bytes = serialize_string_archive_write_bytes;
  self->string = str;
  return &self->super;
}

gboolean
serialize_write_blob(SerializeArchive *archive, const void *blob, gsize len)
{
  return serialize_archive_write_bytes(archive, blob, len);
}

gboolean
serialize_read_blob(SerializeArchive *archive, void *blob, gsize len)
{
  return serialize_archive_read_bytes(archive, blob, len);
}

gboolean
serialize_write_string(SerializeArchive *archive, GString *str)
{
  return serialize_write_uint32(archive, str->len) && 
         serialize_archive_write_bytes(archive, str->str, str->len);
}

gboolean
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

gboolean
serialize_write_cstring(SerializeArchive *archive, const gchar *str, gssize len)
{
  if (len < 0)
    len = strlen(str);
    
  return serialize_write_uint32(archive, len) && 
         (len == 0 || serialize_archive_write_bytes(archive, str, len));
}

gboolean
serialize_read_cstring(SerializeArchive *archive, gchar **str, gsize *strlen)
{
  guint32 len;
  
  if (serialize_read_uint32(archive, &len))
    {
      *str = g_try_malloc(len + 1);
      
      if (!(*str))
        return FALSE;
      (*str)[len] = 0;
      if (strlen)
        *strlen = len;
      return serialize_archive_read_bytes(archive, *str, len);
    }
  return FALSE;
}

gboolean
serialize_write_uint32(SerializeArchive *archive, guint32 value)
{
  guint32 n;
  
  n = GUINT32_TO_BE(value);
  return serialize_archive_write_bytes(archive, (gchar *) &n, sizeof(n));
}

gboolean
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

gboolean
serialize_write_uint64(SerializeArchive *archive, guint64 value)
{
  guint64 n;
  
  n = GUINT64_TO_BE(value);
  return serialize_archive_write_bytes(archive, (gchar *) &n, sizeof(n));
}

gboolean
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


gboolean
serialize_write_uint16(SerializeArchive *archive, guint16 value)
{
  guint16 n;
  
  n = GUINT16_TO_BE(value);
  return serialize_archive_write_bytes(archive, (gchar *) &n, sizeof(n));
}

gboolean
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

gboolean
serialize_write_uint8(SerializeArchive *archive, guint8 value)
{
  guint8 n;
  
  n = value;
  return serialize_archive_write_bytes(archive, (gchar *) &n, sizeof(n));
}

gboolean
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


