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
  gsize ra_left = self->ra.buf_len - self->ra.pos;
  gsize ra_count = count <= ra_left ? count : ra_left;

  if (ra_count > 0)
    {
      /* prepend data from read ahead buffer */
      memcpy(buf, &self->ra.buf[self->ra.pos], ra_count);
      self->ra.pos += ra_count;
      if (self->ra.pos < self->ra.buf_len)
        {
          return ra_count;
        }
    }
  else
    {
      self->ra.buf_len = self->ra.pos = 0;
      errno = EAGAIN;
      return -1;
    }

  buf = ((gchar *) buf) + ra_count;
  count -= ra_count;

  if (count > 0)
    {
      /* need to read more */
      gssize rc = self->read(self, buf, count, aux);
      if (rc < 0)
        {
          if (errno == EAGAIN)
            return ra_count;
          /* error, we put the bytes back to our read_ahead.buf */
          self->ra.pos -= ra_count;
          return rc;
        }
      else
        return rc + ra_count;
    }
  else
    return ra_count;
}


/* NOTE: this would repeat the entire read operation if you invoke it
 * multiple times.  The maximum size of read_ahead is limited by the size of
 * self->ra.buf[]
 */
gssize
log_transport_read_ahead(LogTransport *self, gpointer buf, gsize buflen, gboolean *moved_forward)
{
  gsize buffer_space = MIN(buflen, sizeof(self->ra.buf));
  gsize count = buffer_space > self->ra.buf_len ? buffer_space - self->ra.buf_len : 0;
  gint rc = 0;

  g_assert(buflen <= sizeof(self->ra.buf));

  /* read at the end of the read_ahead buffer */
  if (count > 0)
    {
      rc = self->read(self,
                      &self->ra.buf[self->ra.buf_len],
                      count,
                      NULL);

      if (rc < 0)
        {
          if (moved_forward)
            *moved_forward = FALSE;
          return rc;
        }
    }

  if (moved_forward)
    *moved_forward = rc > 0;

  self->ra.buf_len += rc;

  if (self->ra.buf_len > 0)
    {
      rc = MIN(self->ra.buf_len, buflen);
      memcpy(buf, self->ra.buf, rc);
      return rc;
    }

  return 0;
}


void
log_transport_free_method(LogTransport *s)
{
}

void
log_transport_init_instance(LogTransport *self, const gchar *name, gint fd)
{
  self->name = name;
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
