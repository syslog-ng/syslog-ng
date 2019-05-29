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

#include "transport-socket.h"

#include <errno.h>
#include <unistd.h>

static gssize
log_transport_dgram_socket_read_method(LogTransport *s, gpointer buf, gsize buflen, LogTransportAuxData *aux)
{
  LogTransportDGramSocket *self = (LogTransportDGramSocket *) s;
  gint rc;
  struct mmsghdr msg;

  if (self->msg_cnt == 0)
    {
      do
        {
          rc = recvmmsg(self->super.super.fd, self->msgs, self->batch_size, 0, NULL);
        }
      while (rc == -1 && errno == EINTR);
      if (rc == 0)
        {
          /* DGRAM sockets should never return EOF, they just need to be read again */
          rc = -1;
          errno = EAGAIN;
        }
      else if (rc > 0)
        {
          self->msg_cnt = rc;
        }
    }
  if (self->msg_cnt > self->msg_idx)
    {
      msg = self->msgs[self->msg_idx];
      rc = msg.msg_len;
      memcpy(buf, msg.msg_hdr.msg_iov->iov_base, rc);

      if (aux && msg.msg_hdr.msg_namelen)
        log_transport_aux_data_set_peer_addr_ref(aux, g_sockaddr_new((struct sockaddr *) msg.msg_hdr.msg_name, msg.msg_hdr.msg_namelen));

      ++self->msg_idx;

      if (self->msg_cnt == self->msg_idx)
        {
          self->msg_cnt = 0;
          self->msg_idx = 0;
        }
    }
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
   * error and is calculated in IP statistics, so the best is to handle it as
   * a success.  The only alternative would be to return EAGAIN, which could
   * cause syslog-ng to spin as long as buffer space is unavailable.  Since
   * I'm not sure how much time that would take and I think spinning the CPU
   * is not a good idea in general, I just drop the packet in this case.
   * UDP is lossy anyway */

  if (rc < 0 && errno == ENOBUFS)
    return buflen;
  return rc;
}

void
log_transport_dgram_socket_init_instance(LogTransportSocket *self, gint fd)
{
  log_transport_init_instance(&self->super, fd);
  self->super.read = log_transport_dgram_socket_read_method;
  self->super.write = log_transport_dgram_socket_write_method;
}

LogTransport *
log_transport_dgram_socket_new(gint fd)
{
  LogTransportDGramSocket *self = g_new0(LogTransportDGramSocket, 1);
  struct iovec *iov;
  struct sockaddr *addr;
  gint i;

  self->batch_size = BATCH_SIZE;
  self->buffer_len = BUFFER_LEN;

  self->msgs = (struct mmsghdr *) g_malloc0(sizeof(struct mmsghdr) * self->batch_size);

  for (i=0; i<self->batch_size; i++)
    {
      iov = (struct iovec *) g_malloc0(sizeof(struct iovec));
      if (!iov)
        return NULL;

      iov->iov_base = (gchar *) g_malloc0(self->buffer_len);
      if (!iov->iov_base)
        return NULL;

      iov->iov_len = self->buffer_len;

      addr = (struct sockaddr *) g_malloc0(sizeof(struct sockaddr));
      if (!addr)
        return NULL;

      self->msgs[i].msg_hdr.msg_iov = iov;
      self->msgs[i].msg_hdr.msg_iovlen = 1;
      self->msgs[i].msg_hdr.msg_name = addr;
      self->msgs[i].msg_hdr.msg_namelen = sizeof(addr);
    }

  log_transport_dgram_socket_init_instance(&self->super, fd);
  return &self->super.super;
}

static gssize
log_transport_stream_socket_read_method(LogTransport *s, gpointer buf, gsize buflen, LogTransportAuxData *aux)
{
  LogTransportSocket *self = (LogTransportSocket *) s;
  gint rc;

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
  LogTransportDGramSocket *self = (LogTransportDGramSocket *) s;
  gint i;

  if (s->fd != -1)
    shutdown(s->fd, SHUT_RDWR);

  for (i=0; i<self->batch_size; i++)
    {
      g_free(self->msgs[i].msg_hdr.msg_name);
      g_free(self->msgs[i].msg_hdr.msg_iov->iov_base);
      g_free(self->msgs[i].msg_hdr.msg_iov);
    }

  g_free(self->msgs);

  log_transport_free_method(s);
}

void
log_transport_stream_socket_init_instance(LogTransportSocket *self, gint fd)
{
  log_transport_init_instance(&self->super, fd);
  self->super.read = log_transport_stream_socket_read_method;
  self->super.write = log_transport_stream_socket_write_method;
  self->super.free_fn = log_transport_stream_socket_free_method;
}

LogTransport *
log_transport_stream_socket_new(gint fd)
{
  LogTransportSocket *self = g_new0(LogTransportSocket, 1);

  log_transport_stream_socket_init_instance(self, fd);
  return &self->super;
}
