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
#include "messages.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

static gint
_determine_address_family(gint fd)
{
  struct sockaddr_storage sas;
  socklen_t len = sizeof(sas);

  if (getsockname(fd, (struct sockaddr *) &sas, &len) < 0)
    return 0;
  return sas.ss_family;
}

static gint
_determine_proto_value_based_on_so_protocol(gint fd)
{
#if defined(SO_PROTOCOL)
  gint ipproto;
  socklen_t ipproto_len = sizeof(ipproto);

  /* supported by Linux and FreeBSD */
  if (getsockopt(fd, SOL_SOCKET, SO_PROTOCOL, &ipproto, &ipproto_len) >= 0)
    return ipproto;
#endif
  return 0;
}

static gint
_determine_proto_value_based_on_so_domain_and_type(gint fd, gint address_family)
{
  gint type;
  socklen_t len = sizeof(type);

  if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &len) < 0)
    return 0;

  switch (address_family)
    {
    case AF_INET:
    case AF_INET6:
      if (type == SOCK_DGRAM)
        return IPPROTO_UDP;
      else if (type == SOCK_STREAM)
        return IPPROTO_TCP;
      break;
    case AF_UNIX:
      break;
    default:
      g_assert_not_reached();
    }
  return 0;
}

static gint
_determine_proto(gint fd, gint address_family)
{
  gint result = _determine_proto_value_based_on_so_protocol(fd);
  if (!result)
    result = _determine_proto_value_based_on_so_domain_and_type(fd, address_family);
  return result;
}

gboolean
_extract_timestamp_from_cmsg(struct cmsghdr *cmsg, struct timespec *timestamp)
{
#ifdef SCM_TIMESTAMPNS
  if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_TIMESTAMPNS)
    {
      memcpy(timestamp, CMSG_DATA(cmsg), sizeof(struct timespec));
      return TRUE;
    }
#endif
  return FALSE;
}

void
log_transport_socket_parse_cmsg_method(LogTransportSocket *s, struct cmsghdr *cmsg, LogTransportAuxData *aux)
{
  struct timespec timestamp;
  if (_extract_timestamp_from_cmsg(cmsg, &timestamp))
    {
      log_transport_aux_data_set_timestamp(aux, &timestamp);
      return;
    }
}

static void
_setup_fd(LogTransportSocket *self, gint fd)
{
#ifdef SO_TIMESTAMPNS
  gint on = 1;

  setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPNS, &on, sizeof(on));
#endif
}

#if defined(SYSLOG_NG_HAVE_CTRLBUF_IN_MSGHDR)

static void
_parse_cmsg_to_aux(LogTransportSocket *self, struct msghdr *msg, LogTransportAuxData *aux)
{
  struct cmsghdr *cmsg;

  if (G_UNLIKELY(msg->msg_flags & MSG_CTRUNC))
    {
      msg_warning_once("WARNING: recvmsg() returned truncated control data, the size of the control data buffer needs to be increased",
                       evt_tag_int("control_len", msg->msg_controllen));
      return;
    }

  if (!self->parse_cmsg || !aux)
    return;

  for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg))
    {
      self->parse_cmsg(self, cmsg, aux);
    }
}

#else
#define _parse_cmsg_to_aux(s, m, a)
#endif

static void
_extract_from_msghdr_method(LogTransportSocket *self, struct msghdr *msg, LogTransportAuxData *aux)
{
  if (msg->msg_namelen && aux)
    log_transport_aux_data_set_peer_addr_ref(aux, g_sockaddr_new((struct sockaddr *) msg->msg_name, msg->msg_namelen));
  if (aux)
    aux->proto = self->proto;
  _parse_cmsg_to_aux(self, msg, aux);
}

static gssize
log_transport_socket_read_method(LogTransport *s, gpointer buf, gsize buflen, LogTransportAuxData *aux)
{
  LogTransportSocket *self = (LogTransportSocket *) s;
  gint rc;
  struct msghdr msg;
  struct iovec iov[1];
  struct sockaddr_storage ss;
#if defined(SYSLOG_NG_HAVE_CTRLBUF_IN_MSGHDR)
  gchar ctlbuf[256];
  msg.msg_control = ctlbuf;
  msg.msg_controllen = sizeof(ctlbuf);
#endif

  msg.msg_name = (struct sockaddr *) &ss;
  msg.msg_namelen = sizeof(ss);
  msg.msg_iovlen = 1;
  msg.msg_iov = iov;
  iov[0].iov_base = buf;
  iov[0].iov_len = buflen;

  do
    {
      rc = recvmsg(self->super.fd, &msg, 0);
    }
  while (rc == -1 && errno == EINTR);

  if (rc > 0)
    _extract_from_msghdr_method(self, &msg, aux);

  return rc;
}

static gssize
log_transport_socket_write_method(LogTransport *s, const gpointer buf, gsize buflen)
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
log_transport_socket_init_instance(LogTransportSocket *self, gint fd)
{
  log_transport_init_instance(&self->super, fd);
  self->super.read = log_transport_socket_read_method;
  self->super.write = log_transport_socket_write_method;
  self->address_family = _determine_address_family(fd);
  self->proto = _determine_proto(fd, self->address_family);
  self->parse_cmsg = log_transport_socket_parse_cmsg_method;

  _setup_fd(self, fd);
}

static gssize
log_transport_dgram_socket_read_method(LogTransport *s, gpointer buf, gsize buflen, LogTransportAuxData *aux)
{
  gssize rc = log_transport_socket_read_method(s, buf, buflen, aux);
  if (rc == 0)
    {
      /* DGRAM sockets should never return EOF, they just need to be read again */
      rc = -1;
      errno = EAGAIN;
    }
  return rc;
}

static gssize
log_transport_dgram_socket_write_method(LogTransport *s, const gpointer buf, gsize buflen)
{
  gint rc;

  rc = log_transport_socket_write_method(s, buf, buflen);

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
  log_transport_socket_init_instance(self, fd);
  self->super.read = log_transport_dgram_socket_read_method;
  self->super.write = log_transport_dgram_socket_write_method;
}

LogTransport *
log_transport_dgram_socket_new(gint fd)
{
  LogTransportSocket *self = g_new0(LogTransportSocket, 1);

  log_transport_dgram_socket_init_instance(self, fd);
  return &self->super;
}

void
log_transport_stream_socket_free_method(LogTransport *s)
{
  if (s->fd != -1)
    shutdown(s->fd, SHUT_RDWR);
  log_transport_free_method(s);
}

void
log_transport_stream_socket_init_instance(LogTransportSocket *self, gint fd)
{
  log_transport_socket_init_instance(self, fd);
  self->super.free_fn = log_transport_stream_socket_free_method;
}

LogTransport *
log_transport_stream_socket_new(gint fd)
{
  LogTransportSocket *self = g_new0(LogTransportSocket, 1);

  log_transport_stream_socket_init_instance(self, fd);
  return &self->super;
}
