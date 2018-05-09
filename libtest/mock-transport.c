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
  struct iovec_const iov;
  guint error_code;
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
  /* position within the current I/O chunk */
  gint current_iov_pos;
  gboolean input_is_a_stream;
  gboolean inject_eagain;
  gboolean eof_is_eagain;
  gpointer user_data;
};

LogTransportMock *
log_transport_mock_clone(LogTransportMock *self)
{
  LogTransportMock *clone = g_new(LogTransportMock, 1);
  *clone = *self;
  clone->write_buffer = g_array_new(TRUE, TRUE, 1);
  g_array_append_vals(clone->write_buffer, self->write_buffer->data, self->write_buffer->len);
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

gssize
log_transport_mock_read_from_write_buffer(LogTransportMock *self, gchar *buffer, gsize len)
{
  gsize data_len = MIN(self->write_buffer->len - self->write_buffer_index, len);
  memcpy(buffer, self->write_buffer->data + self->write_buffer_index, data_len);
  self->write_buffer_index += data_len;
  return data_len;
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

static gssize
log_transport_mock_write_method(LogTransport *s, const gpointer buf, gsize count)
{
  LogTransportMock *self = (LogTransportMock *)s;
  g_array_append_vals(self->write_buffer, buf, count);
  return count;
}

static void
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
  data_t data = {.type = DATA_ERROR, .error_code = GPOINTER_TO_UINT(buffer)};
  g_array_append_val(self->value, data);
}

static void
inject_chunk(LogTransportMock *self, const gchar *buffer, gssize length)
{
  if (length == -1)
    length = strlen(buffer);

  data_t data = {.type = DATA_STRING, .iov = (struct iovec_const)
  {
    .iov_base = buffer, .iov_len = length
  }
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

static void
log_transport_mock_init(LogTransportMock *self, const gchar *read_buffer1, gssize read_buffer_length1, va_list va)
{
  const gchar *buffer;
  gssize length;

  log_transport_init_instance(&self->super, 0);
  self->super.read = log_transport_mock_read_method;
  self->super.write = log_transport_mock_write_method;
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
  self->write_buffer = g_array_new(TRUE, TRUE, 1);
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
