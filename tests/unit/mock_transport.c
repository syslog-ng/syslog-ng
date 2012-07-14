#include "mock_transport.h"
#include "gsockaddr.h"

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
} LogTransportMock;

gssize
log_transport_mock_read_method(LogTransport *s, gpointer buf, gsize count, GSockAddr **sa)
{
  LogTransportMock *self = (LogTransportMock *) s;
  struct iovec *current_iov;

  if (self->current_iov_ndx >= self->iov_cnt)
    {
      /* EOF */
      return 0;
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
  if (sa)
    *sa = g_sockaddr_inet_new("1.2.3.4", 5555);
  return count;
}

LogTransport *
log_transport_mock_new(gboolean input_is_a_stream, gchar *read_buffer1, gssize read_buffer_length1, ...)
{
  LogTransportMock *self = g_new0(LogTransportMock, 1);
  gchar *buffer;
  gssize length;
  va_list va;

  self->super.fd = 0;
  self->super.cond = 0;
  self->super.read = log_transport_mock_read_method;
  self->super.free_fn = log_transport_free_method;
  self->input_is_a_stream = input_is_a_stream;

  va_start(va, read_buffer_length1);

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

  va_end(va);
  self->current_iov_ndx = 0;
  self->current_iov_pos = 0;

  return &self->super;
}
