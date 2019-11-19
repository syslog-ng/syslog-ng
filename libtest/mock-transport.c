/*
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Balázs Scheidler
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

#include "mock-transport.h"
#include "gsockaddr.h"

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef enum data_type
{
  DATA_STRING,
  DATA_ERROR,
} data_type_t;

struct iovec_const
{
  const gchar *iov_base;     /* Pointer to data.  */
  gsize iov_len;     /* Length of data.  */
};

typedef struct data
{
  data_type_t type;
  union
  {
    struct iovec_const iov;
    guint error_code;
  };
} data_t;

struct _LogTransportMock
{
  LogTransport super;
  /* data is stored in a series of data chunks, that are going to be returned as individual reads */
  GArray *value;
  /* index currently read i/o chunk */
  GArray *write_buffer;
  gint write_buffer_index;
  gint current_value_ndx;
  gsize write_chunk_limit;
  /* position within the current I/O chunk */
  gint current_iov_pos;
  gboolean input_is_a_stream;
  gboolean inject_eagain;
  gboolean eof_is_eagain;
  gpointer user_data;
};

static void
destroy_write_buffer_element(gpointer d)
{
  data_t *data = (data_t *)d;
  if (data->type == DATA_STRING)
    g_free((gchar *)data->iov.iov_base);
};

static GArray *
clone_write_buffer(GArray *orig_write_buffer)
{
  GArray *write_buffer = g_array_new(TRUE, TRUE, sizeof(data_t));
  g_array_set_clear_func(write_buffer, destroy_write_buffer_element);
  g_array_append_vals(write_buffer, orig_write_buffer->data, orig_write_buffer->len);
  for (int i = 0; i < write_buffer->len; i++)
    {
      data_t *data = &g_array_index(write_buffer, data_t, i);
      if (data->type == DATA_STRING)
        data->iov.iov_base = g_strndup(data->iov.iov_base, data->iov.iov_len);
    }

  return write_buffer;
}

LogTransportMock *
log_transport_mock_clone(LogTransportMock *self)
{
  LogTransportMock *clone = g_new(LogTransportMock, 1);
  *clone = *self;

  clone->write_buffer = clone_write_buffer(self->write_buffer);

  clone->value = g_array_new(TRUE, TRUE, sizeof(data_t));
  g_array_append_vals(clone->value, self->value->data, self->value->len);

  return clone;
}

gpointer
log_transport_mock_get_user_data(LogTransportMock *self)
{
  return self->user_data;
}

void
log_transport_mock_set_user_data(LogTransportMock *self, gpointer user_data)
{
  self->user_data = user_data;
}

static GString *
concat_write_buffer(GArray *write_buffer)
{
  GString *concatenated = g_string_new("");
  for (int i = 0; i < write_buffer->len; i++)
    {
      data_t *data = &g_array_index(write_buffer, data_t, i);
      g_string_append_len(concatenated, data->iov.iov_base, data->iov.iov_len);
    }

  return concatenated;
}

gssize
log_transport_mock_read_from_write_buffer(LogTransportMock *self, gchar *buffer, gsize len)
{
  GString *all_written_data = concat_write_buffer(self->write_buffer);
  gsize data_len = MIN(all_written_data->len - self->write_buffer_index, len);

  memcpy(buffer, all_written_data->str + self->write_buffer_index, data_len);
  self->write_buffer_index += data_len;

  g_string_free(all_written_data, TRUE);
  return data_len;
}

static void
find_chunk(GArray *write_buffer, gsize location, gsize *chunk_index, gsize *chunk_offset)
{

  gsize chunk_location = 0;
  int i;
  for (i = 0; i < write_buffer->len; i++)
    {
      gsize chunk_len = g_array_index(write_buffer, data_t, i).iov.iov_len;
      if (chunk_location + chunk_len > location)
        break;

      chunk_location += chunk_len;
    }

  g_assert(i != write_buffer->len); /* location is too large */

  *chunk_index = i;
  *chunk_offset = location - chunk_location;
}

gssize
log_transport_mock_read_chunk_from_write_buffer(LogTransportMock *self, gchar *buffer)
{
  gsize chunk_index;
  gsize chunk_offset;

  find_chunk(self->write_buffer, self->write_buffer_index, &chunk_index, &chunk_offset);
  data_t data = g_array_index(self->write_buffer, data_t, chunk_index);
  gssize written_len = data.iov.iov_len - chunk_offset;

  memcpy(buffer, data.iov.iov_base + chunk_offset, written_len);
  self->write_buffer_index += written_len;

  return written_len;
}

void
log_transport_mock_set_write_chunk_limit(LogTransportMock *self, gsize chunk_limit)
{
  self->write_chunk_limit = chunk_limit;
}

void
log_transport_mock_empty_write_buffer(LogTransportMock *self)
{
  g_array_set_size(self->write_buffer, 0);
  self->write_buffer_index = 0;
  self->current_value_ndx = 0;
}

gssize
log_transport_mock_read_method(LogTransport *s, gpointer buf, gsize count, LogTransportAuxData *aux)
{
  LogTransportMock *self = (LogTransportMock *) s;
  struct iovec_const *current_iov;

  if (self->current_value_ndx >= self->value->len)
    {
      count = 0;
      goto exit;
    }


  if (self->inject_eagain)
    {
      self->inject_eagain = FALSE;
      errno = EAGAIN;
      return -1;
    }
  else
    {
      self->inject_eagain = TRUE;
    }

  switch (g_array_index(self->value, data_t, self->current_value_ndx).type)
    {
    case DATA_STRING:
      if (self->input_is_a_stream)
        count = 1;

      current_iov = &g_array_index(self->value, data_t, self->current_value_ndx).iov;
      if (count + self->current_iov_pos > current_iov->iov_len)
        count = current_iov->iov_len - self->current_iov_pos;

      memcpy(buf, current_iov->iov_base + self->current_iov_pos, count);
      self->current_iov_pos += count;
      if (self->current_iov_pos >= current_iov->iov_len)
        {
          self->current_iov_pos = 0;
          self->current_value_ndx++;
        }
      break;
    case DATA_ERROR:
      self->current_value_ndx++;
      errno = g_array_index(self->value, data_t, self->current_value_ndx).error_code;
      return -1;
    default:
      g_assert_not_reached();
    }

  if (aux)
    aux->peer_addr = g_sockaddr_inet_new("1.2.3.4", 5555);

exit:
  if (count == 0 && self->eof_is_eagain)
    {
      errno = EAGAIN;
      return -1;
    }
  return count;
}

gssize
log_transport_mock_write_method(LogTransport *s, const gpointer buf, gsize count)
{
  LogTransportMock *self = (LogTransportMock *)s;
  data_t data;

  if (self->write_chunk_limit && self->write_chunk_limit < count)
    count = self->write_chunk_limit;

  data.type = DATA_STRING;
  data.iov.iov_base = g_strndup(buf, count);
  data.iov.iov_len = count;
  g_array_append_val(self->write_buffer, data);

  return count;
}

gssize
log_transport_mock_writev_method(LogTransport *s, struct iovec *iov, gint iov_count)
{
  LogTransportMock *self = (LogTransportMock *)s;

  gssize sum = 0;
  for (gint i = 0; i < iov_count; i++)
    {
      data_t value;

      value.type = DATA_STRING;
      value.iov.iov_len = iov[i].iov_len;

      if (self->write_chunk_limit &&
          sum + iov[i].iov_len > self->write_chunk_limit)
        value.iov.iov_len = self->write_chunk_limit - sum;

      value.iov.iov_base = g_strndup(iov[i].iov_base, value.iov.iov_len);

      g_array_append_val(self->write_buffer, value);
      sum += value.iov.iov_len;
      if (value.iov.iov_len != iov[i].iov_len)
        break;
    }
  return sum;
}

void
log_transport_mock_free_method(LogTransport *s)
{
  LogTransportMock *self = (LogTransportMock *)s;
  g_array_free(self->write_buffer, TRUE);
  g_array_free(self->value, TRUE);
  log_transport_free_method(s);
}

static void
inject_error(LogTransportMock *self, const gchar *buffer, gssize length)
{
  data_t data;
  data.type = DATA_ERROR;
  data.error_code = GPOINTER_TO_UINT(buffer);
  g_array_append_val(self->value, data);
}

static void
inject_chunk(LogTransportMock *self, const gchar *buffer, gssize length)
{
  if (length == -1)
    length = strlen(buffer);

  data_t data;
  data.type = DATA_STRING;
  data.iov = (struct iovec_const)
  {
    .iov_base = buffer, .iov_len = length
  };
  g_array_append_val(self->value, data);

}

void
log_transport_mock_inject_data(LogTransportMock *self, const gchar *buffer, gssize length)
{
  if (length == LTM_INJECT_ERROR_LENGTH)
    inject_error(self, buffer, length);
  else
    inject_chunk(self, buffer, length);
}

void
log_transport_mock_init(LogTransportMock *self, const gchar *read_buffer1, gssize read_buffer_length1, va_list va)
{
  const gchar *buffer;
  gssize length;

  log_transport_init_instance(&self->super, 0);
  self->super.read = log_transport_mock_read_method;
  self->super.write = log_transport_mock_write_method;
  self->super.writev = log_transport_mock_writev_method;
  self->super.free_fn = log_transport_mock_free_method;

  buffer = read_buffer1;
  length = read_buffer_length1;
  while (buffer)
    {
      log_transport_mock_inject_data(self, buffer, length);
      buffer = va_arg(va, const gchar *);
      length = va_arg(va, gint);
    }

  self->current_value_ndx = 0;
  self->current_iov_pos = 0;
}

LogTransportMock *
log_transport_mock_new(void)
{
  LogTransportMock *self = g_new0(LogTransportMock, 1);
  self->write_buffer = g_array_new(TRUE, TRUE, sizeof(data_t));
  g_array_set_clear_func(self->write_buffer, destroy_write_buffer_element);
  self->value = g_array_new(TRUE, TRUE, sizeof(data_t));
  return self;
}

LogTransport *
log_transport_mock_stream_new(const gchar *read_buffer1, gssize read_buffer_length1, ...)
{
  LogTransportMock *self = log_transport_mock_new();
  va_list va;

  va_start(va, read_buffer_length1);
  log_transport_mock_init(self, read_buffer1, read_buffer_length1, va);
  va_end(va);
  self->input_is_a_stream = TRUE;
  return &self->super;
}

LogTransport *
log_transport_mock_endless_stream_new(const gchar *read_buffer1, gssize read_buffer_length1, ...)
{
  LogTransportMock *self = log_transport_mock_new();
  va_list va;

  va_start(va, read_buffer_length1);
  log_transport_mock_init(self, read_buffer1, read_buffer_length1, va);
  va_end(va);
  self->input_is_a_stream = TRUE;
  self->eof_is_eagain = TRUE;
  return &self->super;
}

LogTransport *
log_transport_mock_records_new(const gchar *read_buffer1, gssize read_buffer_length1, ...)
{
  LogTransportMock *self = log_transport_mock_new();
  va_list va;

  va_start(va, read_buffer_length1);
  log_transport_mock_init(self, read_buffer1, read_buffer_length1, va);
  va_end(va);
  return &self->super;
}

LogTransport *
log_transport_mock_endless_records_new(const gchar *read_buffer1, gssize read_buffer_length1, ...)
{
  LogTransportMock *self = log_transport_mock_new();
  va_list va;

  va_start(va, read_buffer_length1);
  log_transport_mock_init(self, read_buffer1, read_buffer_length1, va);
  va_end(va);
  self->eof_is_eagain = TRUE;
  return &self->super;
}
