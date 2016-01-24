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

typedef struct
{
  LogTransport super;
  /* data is stored in a series of data chunks, that are going to be returned as individual reads */
  struct iovec iov[32];
  gint iov_cnt;
  /* index currently read i/o chunk */
  gint current_iov_ndx;
  /* position within the current I/O chunk */
  gint current_iov_pos;
  gboolean input_is_a_stream;
  gboolean inject_eagain;
  gboolean eof_is_eagain;
} LogTransportMock;

gssize
log_transport_mock_read_method(LogTransport *s, gpointer buf, gsize count, LogTransportAuxData *aux)
{
  LogTransportMock *self = (LogTransportMock *) s;
  struct iovec *current_iov;

  if (self->current_iov_ndx >= self->iov_cnt)
    {
      count = 0;
      goto exit;
    }

  current_iov = &self->iov[self->current_iov_ndx];

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
  if (self->input_is_a_stream)
    count = 1;

  if (count + self->current_iov_pos > current_iov->iov_len)
    count = current_iov->iov_len - self->current_iov_pos;

  if (GPOINTER_TO_UINT(current_iov->iov_base) < 4096)
    {
      /* error injection */
      errno = GPOINTER_TO_UINT(current_iov->iov_base);
      return -1;
    }

  memcpy(buf, current_iov->iov_base + self->current_iov_pos, count);
  self->current_iov_pos += count;
  if (self->current_iov_pos >= current_iov->iov_len)
    {
      self->current_iov_pos = 0;
      self->current_iov_ndx++;
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

static void
log_transport_mock_init(LogTransportMock *self, gchar *read_buffer1, gssize read_buffer_length1, va_list va)
{
  gchar *buffer;
  gssize length;

  self->super.fd = 0;
  self->super.cond = 0;
  self->super.read = log_transport_mock_read_method;
  self->super.free_fn = log_transport_free_method;

  buffer = read_buffer1;
  length = read_buffer_length1;
  while (buffer)
    {
      /* NOTE: our iov buffer is of a fixed size, increase if your test needs more chunks of data */
      g_assert(self->iov_cnt < sizeof(self->iov) / sizeof(self->iov[0]));

      if (length < 0)
        length = strlen(buffer);

      self->iov[self->iov_cnt].iov_base = buffer;
      self->iov[self->iov_cnt].iov_len = length;
      self->iov_cnt++;
      buffer = va_arg(va, gchar *);
      length = va_arg(va, gint);
    }

  self->current_iov_ndx = 0;
  self->current_iov_pos = 0;
}

LogTransport *
log_transport_mock_stream_new(gchar *read_buffer1, gssize read_buffer_length1, ...)
{
  LogTransportMock *self = g_new0(LogTransportMock, 1);
  va_list va;

  va_start(va, read_buffer_length1);
  log_transport_mock_init(self, read_buffer1, read_buffer_length1, va);
  va_end(va);
  self->input_is_a_stream = TRUE;
  return &self->super;
}

LogTransport *
log_transport_mock_endless_stream_new(gchar *read_buffer1, gssize read_buffer_length1, ...)
{
  LogTransportMock *self = g_new0(LogTransportMock, 1);
  va_list va;

  va_start(va, read_buffer_length1);
  log_transport_mock_init(self, read_buffer1, read_buffer_length1, va);
  va_end(va);
  self->input_is_a_stream = TRUE;
  self->eof_is_eagain = TRUE;
  return &self->super;
}

LogTransport *
log_transport_mock_records_new(gchar *read_buffer1, gssize read_buffer_length1, ...)
{
  LogTransportMock *self = g_new0(LogTransportMock, 1);
  va_list va;

  va_start(va, read_buffer_length1);
  log_transport_mock_init(self, read_buffer1, read_buffer_length1, va);
  va_end(va);
  return &self->super;
}

LogTransport *
log_transport_mock_endless_records_new(gchar *read_buffer1, gssize read_buffer_length1, ...)
{
  LogTransportMock *self = g_new0(LogTransportMock, 1);
  va_list va;

  va_start(va, read_buffer_length1);
  log_transport_mock_init(self, read_buffer1, read_buffer_length1, va);
  va_end(va);
  self->eof_is_eagain = TRUE;
  return &self->super;
}
