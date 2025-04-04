/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 1998-2013 Balázs Scheidler
 * Copyright (c) 2020-2024 László Várady
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

#include "transport/transport-tls.h"
#include "transport/transport-socket.h"

#include "messages.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <errno.h>

typedef struct _LogTransportTLS
{
  LogTransportSocket super;
  TLSSession *tls_session;
  gboolean sending_shutdown;
} LogTransportTLS;

static inline gboolean
_is_shutdown_sent(gint shutdown_rc)
{
  return shutdown_rc >= 0;
}

static inline void
_handle_shutdown_error(LogTransportTLS *self, gint ssl_error)
{
  switch (ssl_error)
    {
    case SSL_ERROR_WANT_READ:
      self->super.super.cond = G_IO_IN;
      errno = EAGAIN;
      break;
    case SSL_ERROR_WANT_WRITE:
      self->super.super.cond = G_IO_OUT;
      errno = EAGAIN;
      break;
    case SSL_ERROR_SYSCALL:
      /* errno is set accordingly */
      self->sending_shutdown = FALSE;
      break;
    default:
      msg_error("SSL error while shutting down stream",
                tls_context_format_tls_error_tag(self->tls_session->ctx),
                tls_context_format_location_tag(self->tls_session->ctx));
      ERR_clear_error();
      errno = ECONNRESET;
      self->sending_shutdown = FALSE;
      break;
    }
}

static gint
log_transport_tls_send_shutdown(LogTransportTLS *self)
{
  self->sending_shutdown = TRUE;
  gint shutdown_rc = SSL_shutdown(self->tls_session->ssl);

  if (_is_shutdown_sent(shutdown_rc))
    {
      self->sending_shutdown = FALSE;
      return shutdown_rc;
    }

  gint ssl_error = SSL_get_error(self->tls_session->ssl, shutdown_rc);
  _handle_shutdown_error(self, ssl_error);

  return shutdown_rc;
}

static gssize
log_transport_tls_read_method(LogTransport *s, gpointer buf, gsize buflen, LogTransportAuxData *aux)
{
  LogTransportTLS *self = (LogTransportTLS *) s;
  gint ssl_error;
  gint rc;

  if (G_UNLIKELY(self->sending_shutdown))
    return (log_transport_tls_send_shutdown(self) >= 0) ? 0 : -1;

  /* assume that we need to poll our input for reading unless
   * SSL_ERROR_WANT_WRITE is specified by libssl */
  self->super.super.cond = G_IO_IN;

  if (aux)
    {
      /* if we have found the peer has a certificate */
      if (self->tls_session->peer_info.found)
        {
          log_transport_aux_data_add_nv_pair(aux, ".tls.x509_cn", self->tls_session->peer_info.cn);
          log_transport_aux_data_add_nv_pair(aux, ".tls.x509_o", self->tls_session->peer_info.o);
          log_transport_aux_data_add_nv_pair(aux, ".tls.x509_ou", self->tls_session->peer_info.ou);
        }
      if (self->tls_session->peer_info.fingerprint[0])
        log_transport_aux_data_add_nv_pair(aux, ".tls.x509_fp", self->tls_session->peer_info.fingerprint);

      /* NOTE: we only support TLS on top of TCP for now.  We could reuse the
       * proto auto detection code from transport-socket to make this more
       * accurate.  */

      aux->proto = IPPROTO_TCP;
    }
  do
    {
      rc = SSL_read(self->tls_session->ssl, buf, buflen);

      if (rc <= 0)
        {
          ssl_error = SSL_get_error(self->tls_session->ssl, rc);
          switch (ssl_error)
            {
            case SSL_ERROR_WANT_READ:
              rc = -1;
              errno = EAGAIN;
              break;
            case SSL_ERROR_WANT_WRITE:
              /* although we are reading this fd, libssl wants to write. This
               * happens during renegotiation for example */
              self->super.super.cond = G_IO_OUT;
              rc = -1;
              errno = EAGAIN;
              break;
            case SSL_ERROR_ZERO_RETURN:
              rc = (log_transport_tls_send_shutdown(self) >= 0) ? 0 : -1;
              break;
            case SSL_ERROR_SYSCALL:
              // https://github.com/openssl/openssl/pull/11400
              // There is a known bug in OpenSSL where it reports SSL_ERROR_SYSCALL without setting
              // the proper errno value. The mentioned PR were reverted because lot of legacy code
              // were broken by the fix. OpenSSL 3.0.0 will contain it.
              rc = (errno == 0) ? 0 : -1;
              break;
            default:
              goto tls_error;
            }
        }
    }
  while (rc == -1 && errno == EINTR);

  if (rc > 0)
    self->super.super.cond = 0;

  return rc;
tls_error:

  msg_error("SSL error while reading stream",
            tls_context_format_tls_error_tag(self->tls_session->ctx),
            tls_context_format_location_tag(self->tls_session->ctx));
  ERR_clear_error();

  errno = ECONNRESET;
  return -1;

}

static gssize
log_transport_tls_write_method(LogTransport *s, const gpointer buf, gsize buflen)
{
  LogTransportTLS *self = (LogTransportTLS *) s;
  gint ssl_error;
  gint rc;

  /* assume that we need to poll our output for writing unless
   * SSL_ERROR_WANT_READ is specified by libssl */

  self->super.super.cond = G_IO_OUT;

  rc = SSL_write(self->tls_session->ssl, buf, buflen);

  if (rc < 0)
    {
      ssl_error = SSL_get_error(self->tls_session->ssl, rc);
      switch (ssl_error)
        {
        case SSL_ERROR_WANT_READ:
          /* although we are writing this fd, libssl wants to read. This
           * happens during renegotiation for example */
          self->super.super.cond = G_IO_IN;
          errno = EAGAIN;
          break;
        case SSL_ERROR_WANT_WRITE:
          errno = EAGAIN;
          break;
        case SSL_ERROR_SYSCALL:
          /* errno is set accordingly */

          // https://github.com/openssl/openssl/pull/11400
          // There is a known bug in OpenSSL where it reports SSL_ERROR_SYSCALL without setting
          // the proper errno value. The mentioned PR were reverted because lot of legacy code
          // were broken by the fix. OpenSSL 3.0.0 will contain it.
          if (errno == 0)
            {
              rc = -1;
              errno = ECONNRESET;
            }
          break;
        default:
          goto tls_error;
        }
    }
  else
    {
      self->super.super.cond = 0;
    }

  return rc;

tls_error:

  msg_error("SSL error while writing stream",
            tls_context_format_tls_error_tag(self->tls_session->ctx),
            tls_context_format_location_tag(self->tls_session->ctx));
  ERR_clear_error();

  errno = EPIPE;
  return -1;
}


static void log_transport_tls_free_method(LogTransport *s);

LogTransport *
log_transport_tls_new(TLSSession *tls_session, gint fd)
{
  LogTransportTLS *self = g_new0(LogTransportTLS, 1);

  log_transport_stream_socket_init_instance(&self->super, fd);
  self->super.super.name = "tls";
  self->super.super.cond = 0;
  self->super.super.read = log_transport_tls_read_method;
  self->super.super.write = log_transport_tls_write_method;
  self->super.super.free_fn = log_transport_tls_free_method;
  self->tls_session = tls_session;

  SSL_set_fd(self->tls_session->ssl, fd);
  return &self->super.super;
}

static void
log_transport_tls_free_method(LogTransport *s)
{
  LogTransportTLS *self = (LogTransportTLS *) s;

  /* TODO: should handle SSL_ERROR_WANT_* and retry */
  if (!SSL_in_init(self->tls_session->ssl))
    log_transport_tls_send_shutdown(self);

  tls_session_free(self->tls_session);
  log_transport_stream_socket_free_method(s);
}
