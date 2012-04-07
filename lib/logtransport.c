/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
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

#include "logtransport.h"
#include "messages.h"
#include "alarms.h"

#include <errno.h>
#include <ctype.h>

void
log_transport_free_method(LogTransport *s)
{
  if (((s->flags & LTF_DONTCLOSE) == 0) && s->fd != -1)
    {
      msg_verbose("Closing log transport fd",
                  evt_tag_int("fd", s->fd),
                  NULL);
      if (s->flags & LTF_SHUTDOWN)
        shutdown(s->fd, SHUT_RDWR);
      close(s->fd);
    }
}

void
log_transport_free(LogTransport *self)
{
  self->free_fn(self);
  g_free(self);
}

/* log transport that simply sends messages to an fd */
typedef struct _LogTransportPlain LogTransportPlain;

struct _LogTransportPlain
{
  LogTransport super;
};

static gssize
log_transport_plain_read_method(LogTransport *s, gpointer buf, gsize buflen, GSockAddr **sa)
{
  LogTransportPlain *self = (LogTransportPlain *) s;
  gint rc;
  
  if ((self->super.flags & LTF_RECV) == 0)
    {
      if (sa)
        *sa = NULL;

      do
        {
          if (self->super.timeout)
            alarm_set(self->super.timeout);
          rc = read(self->super.fd, buf, buflen);
          
          if (self->super.timeout > 0 && rc == -1 && errno == EINTR && alarm_has_fired())
            {
              msg_notice("Nonblocking read has blocked, returning with an error",
                         evt_tag_int("fd", self->super.fd),
                         evt_tag_int("timeout", self->super.timeout),
                         NULL);
              alarm_cancel();
              break;
            }
          if (self->super.timeout)
            alarm_cancel();
        }
      while (rc == -1 && errno == EINTR);
    }
  else 
    {
      union
      {
#if HAVE_STRUCT_SOCKADDR_STORAGE
        struct sockaddr_storage __sas;
#endif
        struct sockaddr __sa;
      } sas;
      
      socklen_t salen = sizeof(sas);

      do
        {
          rc = recvfrom(self->super.fd, buf, buflen, 0, 
                        (struct sockaddr *) &sas, &salen);
        }
      while (rc == -1 && errno == EINTR);
      if (rc != -1 && salen && sa)
        (*sa) = g_sockaddr_new((struct sockaddr *) &sas, salen);
    }
  return rc;
}

static gssize
log_transport_plain_write_method(LogTransport *s, const gpointer buf, gsize buflen)
{
  LogTransportPlain *self = (LogTransportPlain *) s;
  gint rc;
  
  do
    {
      if (self->super.timeout)
        alarm_set(self->super.timeout);
      if (self->super.flags & LTF_APPEND)
        lseek(self->super.fd, 0, SEEK_END);

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
      while ((buflen >>= 1) && (self->super.flags & LTF_PIPE) && rc < 0 && errno == EAGAIN);
#else
      while (0);
#endif

      if (self->super.timeout > 0 && rc == -1 && errno == EINTR && alarm_has_fired())
        {
          msg_notice("Nonblocking write has blocked, returning with an error",
                     evt_tag_int("fd", self->super.fd),
                     evt_tag_int("timeout", self->super.timeout),
                     NULL);
          alarm_cancel();
          break;
        }
      if (self->super.timeout)
        alarm_cancel();
      if (self->super.flags & LTF_FSYNC)
        fsync(self->super.fd);
    }
  while (rc == -1 && errno == EINTR);

  /* NOTE: FreeBSD returns ENOBUFS on send() failure instead of indicating
   * this conditions via poll().  The return of ENOBUFS actually is a send
   * error and is calulated in IP statistics, so the best is to handle it as
   * a success.  The only alternative would be to return EAGAIN, which could
   * cause syslog-ng to spin as long as buffer space is unavailable.  Since
   * I'm not sure how much time that would take and I think spinning the CPU
   * is not a good idea in general, I just drop the packet in this case.
   * UDP is lossy anyway */

  if (rc < 0 && errno == ENOBUFS)
    return buflen;
  return rc;
}


LogTransport *
log_transport_plain_new(gint fd, guint flags)
{
  LogTransportPlain *self = g_new0(LogTransportPlain, 1);
  
  self->super.fd = fd;
  self->super.cond = 0;
  self->super.flags = flags;
  self->super.read = log_transport_plain_read_method;
  self->super.write = log_transport_plain_write_method;
  self->super.free_fn = log_transport_free_method;
  return &self->super;
}


