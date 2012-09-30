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
  if (s->fd != -1)
    {
      msg_verbose("Closing log transport fd",
                  evt_tag_int("fd", s->fd),
                  NULL);
      close(s->fd);
    }
}

void
log_transport_init_method(LogTransport *self, gint fd)
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

/* log transport that simply sends messages to an fd */
typedef struct _LogTransportFile LogTransportFile;
struct _LogTransportFile
{
  LogTransport super;
};

static gssize
log_transport_file_read_method(LogTransport *s, gpointer buf, gsize buflen, GSockAddr **sa)
{
  LogTransportFile *self = (LogTransportFile *) s;
  gint rc;
  
  if (sa)
    *sa = NULL;

  do
    {
      rc = read(self->super.fd, buf, buflen);
    }
  while (rc == -1 && errno == EINTR);

  if (rc == 0)
    {
      /* regular files should never return EOF, they just need to be read again */
      rc = -1;
      errno = EAGAIN;
    }
  return rc;
}

static gssize
log_transport_file_write_method(LogTransport *s, const gpointer buf, gsize buflen)
{
  LogTransportFile *self = (LogTransportFile *) s;
  gint rc;
  
  do
    {
      rc = write(self->super.fd, buf, buflen);
    }
  while (rc == -1 && errno == EINTR);
  return rc;
}

static gssize
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

/* regular files */
LogTransport *
log_transport_file_new(gint fd)
{
  LogTransportFile *self = g_new0(LogTransportFile, 1);

  log_transport_init_method(&self->super, fd);
  self->super.read = log_transport_file_read_method;
  self->super.write = log_transport_file_write_method;
  self->super.free_fn = log_transport_free_method;
  return &self->super;
}

LogTransport *
log_transport_pipe_new(gint fd)
{
  LogTransportFile *self = g_new0(LogTransportFile, 1);

  log_transport_init_method(&self->super, fd);
  self->super.read = log_transport_file_read_method;
  self->super.write = log_transport_pipe_write_method;
  self->super.free_fn = log_transport_free_method;
  return &self->super;
}

typedef struct _LogTransportDevice LogTransportDevice;
struct _LogTransportDevice
{
  LogTransport super;
  gint timeout;
};

static gssize
log_transport_device_read_method(LogTransport *s, gpointer buf, gsize buflen, GSockAddr **sa)
{
  LogTransportDevice *self = (LogTransportDevice *) s;
  gint rc;

  if (sa)
    *sa = NULL;

  do
    {
      if (self->timeout)
        alarm_set(self->timeout);
      rc = read(self->super.fd, buf, buflen);

      if (self->timeout > 0 && rc == -1 && errno == EINTR && alarm_has_fired())
        {
          msg_notice("Nonblocking read has blocked, returning with an error",
                     evt_tag_int("fd", self->super.fd),
                     evt_tag_int("timeout", self->timeout),
                     NULL);
          alarm_cancel();
          break;
        }
      if (self->timeout)
        alarm_cancel();
    }
  while (rc == -1 && errno == EINTR);
  return rc;
}

LogTransport *
log_transport_device_new(gint fd, gint timeout)
{
  LogTransportDevice *self = g_new0(LogTransportDevice, 1);

  log_transport_init_method(&self->super, fd);
  self->timeout = timeout;
  self->super.read = log_transport_device_read_method;
  self->super.write = NULL;
  self->super.free_fn = log_transport_free_method;
  return &self->super;
}

typedef struct _LogTransportSocket LogTransportSocket;
struct _LogTransportSocket
{
  LogTransport super;
};

static gssize
log_transport_dgram_socket_read_method(LogTransport *s, gpointer buf, gsize buflen, GSockAddr **sa)
{
  LogTransportSocket *self = (LogTransportSocket *) s;
  gint rc;

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
  return rc;
}

static gssize
log_transport_dgram_socket_write_method(LogTransport *s, const gpointer buf, gsize buflen)
{
  LogTransportSocket *self = (LogTransportSocket *) s;
  gint rc;

  do
    {
      rc = send(self->super.fd, buf, buflen, 0);
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
log_transport_dgram_socket_new(gint fd)
{
  LogTransportSocket *self = g_new0(LogTransportSocket, 1);
  
  log_transport_init_method(&self->super, fd);
  self->super.read = log_transport_dgram_socket_read_method;
  self->super.write = log_transport_dgram_socket_write_method;
  return &self->super;
}

static gssize
log_transport_stream_socket_read_method(LogTransport *s, gpointer buf, gsize buflen, GSockAddr **sa)
{
  LogTransportSocket *self = (LogTransportSocket *) s;
  gint rc;

  if (sa)
    *sa = NULL;
  do
    {
      rc = recv(self->super.fd, buf, buflen, 0);
    }
  while (rc == -1 && errno == EINTR);
  return rc;
}

static gssize
log_transport_stream_socket_write_method(LogTransport *s, const gpointer buf, gsize buflen)
{
  LogTransportSocket *self = (LogTransportSocket *) s;
  gint rc;

  do
    {
      rc = send(self->super.fd, buf, buflen, 0);
    }
  while (rc == -1 && errno == EINTR);
  return rc;
}

static void
log_transport_stream_socket_free_method(LogTransport *s)
{
  shutdown(s->fd, SHUT_RDWR);
  log_transport_free_method(s);
}

LogTransport *
log_transport_stream_socket_new(gint fd)
{
  LogTransportSocket *self = g_new0(LogTransportSocket, 1);

  log_transport_init_method(&self->super, fd);
  self->super.read = log_transport_stream_socket_read_method;
  self->super.write = log_transport_stream_socket_write_method;
  self->super.free_fn = log_transport_stream_socket_free_method;
  return &self->super;
}


