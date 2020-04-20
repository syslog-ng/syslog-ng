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
 */

#include "transport/transport-pipe.h"
#include "transport/transport-file.h"

#include <errno.h>
#include <unistd.h>

gssize
log_transport_pipe_write_method(LogTransport *s, const gpointer buf, gsize buflen)
{
  LogTransportFile *self = (LogTransportFile *) s;
  gint rc;

  do
    {
      /* NOTE: this loop is needed because of the funny semantics that
       * pipe() uses. A pipe has a buffer (sized PIPE_BUF), which
       * determines how much data it can buffer. If the data to be
       * written would overflow the buffer, it may reject it with
       * rc == -1 and errno set to EAGAIN.
       *
       * The issue is worse as AIX may indicate in poll() that the
       * pipe is writable, and then the pipe may flat out reject our
       * write() operation, resulting in a busy loop.
       *
       * The work around is to try to write the data in
       * ever-decreasing size, and only accept EAGAIN if a single byte
       * write is refused as well.
       *
       * Most UNIX platforms behaves better than that, but the AIX
       * implementation is still conforming, for now we only enable it
       * on AIX.
       */

      do
        {
          rc = write(self->super.fd, buf, buflen);
        }
#ifdef __aix__
      while ((buflen >>= 1) && rc < 0 && errno == EAGAIN);
#else
      while (0);
#endif
    }
  while (rc == -1 && errno == EINTR);
  return rc;
}


LogTransport *
log_transport_pipe_new(gint fd)
{
  LogTransportFile *self = g_new0(LogTransportFile, 1);

  log_transport_file_init_instance(self, fd);
  self->super.write = log_transport_pipe_write_method;
  return &self->super;
}
