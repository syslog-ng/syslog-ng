/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#include "logtransport.h"
#include "messages.h"

#include <unistd.h>

gssize
_log_transport_combined_read_with_read_ahead(LogTransport *self,
                                             gpointer buf, gsize count,
                                             LogTransportAuxData *aux)
{
  /* prepend the read ahead buflen to the output buffer */
  g_assert(count >= self->read_ahead_buf_len);

  memcpy(buf, self->read_ahead_buf, self->read_ahead_buf_len);
  gint read_ahead_shifted = self->read_ahead_buf_len;
  self->read_ahead_buf_len = 0;

  buf = ((gchar *) buf) + read_ahead_shifted;
  count -= read_ahead_shifted;

  if (count > 0)
    {
      /* need to read more */
      gssize rc = self->read(self, buf, count, aux);
      if (rc < 0)
        {
          /* error, we put the bytes back to our read_ahead_buf */
          self->read_ahead_buf_len = read_ahead_shifted;
          return rc;
        }
      else
        return rc + read_ahead_shifted;
    }
  else
    return read_ahead_shifted;
}


/* NOTE: this would repeat the entire read operation if you invoke it
 * multiple times.  The maximum size of read_ahead is limited by the size of
 * self->read_ahead_buf[]
 */
gssize
log_transport_read_ahead(LogTransport *self, gpointer buf, gsize buflen)
{
  g_assert(buflen < sizeof(self->read_ahead_buf));
  gint rc = 0;

  while (buflen > self->read_ahead_buf_len)
    {
      /* read at the end of the read_ahead buffer */
      rc = self->read(self,
                      &self->read_ahead_buf[self->read_ahead_buf_len],
                      MIN(buflen, sizeof(self->read_ahead_buf)) - self->read_ahead_buf_len,
                      NULL);

      if (rc < 0 && errno == EAGAIN)
        break;

      if (rc <= 0)
        return rc;
      self->read_ahead_buf_len += rc;
    }

  if (self->read_ahead_buf_len > 0)
    {
      rc = MIN(self->read_ahead_buf_len, buflen);
      memcpy(buf, self->read_ahead_buf, rc);
      return rc;
    }
  errno = EAGAIN;
  return -1;
}


void
log_transport_free_method(LogTransport *s)
{
  if (s->fd != -1)
    {
      msg_trace("Closing log transport fd",
                evt_tag_int("fd", s->fd));
      close(s->fd);
    }
}

void
log_transport_init_instance(LogTransport *self, gint fd)
{
  self->fd = fd;
  self->cond = 0;
  self->free_fn = log_transport_free_method;
}

void
log_transport_free(LogTransport *self)
{
  self->free_fn(self);
  g_free(self);
}

gint
log_transport_release_fd(LogTransport *s)
{
  gint fd = s->fd;
  s->fd = -1;

  return fd;
}

