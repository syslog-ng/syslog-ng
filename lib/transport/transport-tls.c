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
#include "transport/transport-adapter.h"

#include "messages.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <errno.h>

typedef struct _LogTransportTLS
{
  LogTransportAdapter super;
  TLSSession *tls_session;
  gboolean sending_shutdown;
} LogTransportTLS;

const gchar *TLS_TRANSPORT_NAME = "tls";
static BIO_METHOD *meth_transport = NULL;

static int
_BIO_transport_write(BIO *bio, const char *buf, size_t buflen, size_t *written_bytes)
{
  LogTransportTLS *transport = BIO_get_data(bio);
  gssize ret;

  BIO_clear_retry_flags(bio);

  ret = log_transport_adapter_write_method(&transport->super.super, (gpointer) buf, buflen);

  if (ret < 0)
    {
      *written_bytes = 0;
      if (errno == EAGAIN || errno == EINTR)
        BIO_set_retry_write(bio);
      return -1;
    }
  *written_bytes = ret;
  return 1;
}

static int
_BIO_transport_read(BIO *bio, char *buf, size_t buflen, size_t *read_bytes)
{
  LogTransportTLS *transport = BIO_get_data(bio);
  gssize ret;

  BIO_clear_retry_flags(bio);

  ret = log_transport_adapter_read_method(&transport->super.super, buf, buflen, NULL);

  if (ret < 0)
    {
      *read_bytes = 0;
      if (errno == EAGAIN || errno == EINTR)
        BIO_set_retry_read(bio);
      return -1;
    }
  *read_bytes = ret;
  return 1;
}

static long
_BIO_transport_ctrl(BIO *bio, int cmd, long num, void *ptr)
{
  long ret = 1;

  switch (cmd)
    {
    case BIO_CTRL_GET_CLOSE:
      ret = BIO_get_shutdown(bio);
      break;
    case BIO_CTRL_SET_CLOSE:
      BIO_set_shutdown(bio, (int)num);
      break;
    case BIO_CTRL_DUP:
    case BIO_CTRL_FLUSH:
      ret = 1;
      break;
    case BIO_CTRL_RESET:
    case BIO_C_FILE_SEEK:
    case BIO_C_FILE_TELL:
    case BIO_CTRL_INFO:
    case BIO_C_SET_FD:
    case BIO_C_GET_FD:
    case BIO_CTRL_PENDING:
    case BIO_CTRL_WPENDING:
    default:
      ret = 0;
      break;
    }

  return ret;
}

static BIO_METHOD *
BIO_s_transport(void)
{
  BIO_METHOD *meth = BIO_meth_new(BIO_TYPE_NONE, "LogTransportBIO");

  BIO_meth_set_write_ex(meth, _BIO_transport_write);
  BIO_meth_set_read_ex(meth, _BIO_transport_read);
  BIO_meth_set_ctrl(meth, _BIO_transport_ctrl);

  return meth;
}

static BIO *
BIO_transport_new(LogTransportTLS *transport)
{
  BIO *bio = BIO_new(meth_transport);

  g_assert(transport != NULL);
  BIO_set_data(bio, transport);
  return bio;
}

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
      self->super.super.cond = LTIO_READ_WANTS_READ;
      errno = EAGAIN;
      break;
    case SSL_ERROR_WANT_WRITE:
      self->super.super.cond = LTIO_READ_WANTS_WRITE;
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

  self->super.super.cond = LTIO_NOTHING;

  if (G_UNLIKELY(self->sending_shutdown))
    return (log_transport_tls_send_shutdown(self) >= 0) ? 0 : -1;

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
              /* SSL_ERROR_WANT_READ is not a must, LTIO_READ_WANTS_READ should NOT be set */
              rc = -1;
              errno = EAGAIN;
              break;
            case SSL_ERROR_WANT_WRITE:
              /* although we are reading this fd, libssl wants to write. This
               * happens during renegotiation for example */
              self->super.super.cond = LTIO_READ_WANTS_WRITE;
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

  self->super.super.cond = LTIO_NOTHING;

  rc = SSL_write(self->tls_session->ssl, buf, buflen);

  if (rc < 0)
    {
      ssl_error = SSL_get_error(self->tls_session->ssl, rc);
      switch (ssl_error)
        {
        case SSL_ERROR_WANT_READ:
          /* although we are writing this fd, libssl wants to read. This
           * happens during renegotiation for example */
          self->super.super.cond = LTIO_WRITE_WANTS_READ;
          errno = EAGAIN;
          break;
        case SSL_ERROR_WANT_WRITE:
          /*
           * If you get SSL_ERROR_WANT_WRITE from SSL_write() then you should
           * not do any other operation that could trigger IO other than to
           * repeat the previous SSL_write() call.
           */
          self->super.super.cond = LTIO_WRITE_WANTS_WRITE;
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

  return rc;

tls_error:

  msg_error("SSL error while writing stream",
            tls_context_format_tls_error_tag(self->tls_session->ctx),
            tls_context_format_location_tag(self->tls_session->ctx));
  ERR_clear_error();

  errno = EPIPE;
  return -1;
}

TLSSession *
log_tansport_tls_get_session(LogTransport *s)
{
  g_assert(s->name == TLS_TRANSPORT_NAME);

  LogTransportTLS *self = (LogTransportTLS *)s;
  return self->tls_session;
}

static void
log_transport_tls_shutdown_method(LogTransport *s)
{
  LogTransportTLS *self = (LogTransportTLS *) s;

  /* TODO: should handle SSL_ERROR_WANT_* and retry */
  if (!SSL_in_init(self->tls_session->ssl))
    log_transport_tls_send_shutdown(self);
  log_transport_adapter_shutdown_method(s);
}

static void log_transport_tls_free_method(LogTransport *s);

LogTransport *
log_transport_tls_new(TLSSession *tls_session, LogTransportIndex base)
{
  LogTransportTLS *self = g_new0(LogTransportTLS, 1);

  log_transport_adapter_init_instance(&self->super, TLS_TRANSPORT_NAME, base);
  self->super.super.cond = LTIO_NOTHING;
  self->super.super.read = log_transport_tls_read_method;
  self->super.super.write = log_transport_tls_write_method;
  self->super.super.shutdown = log_transport_tls_shutdown_method;
  self->super.super.free_fn = log_transport_tls_free_method;
  self->tls_session = tls_session;

  BIO *bio = BIO_transport_new(self);
  SSL_set_bio(self->tls_session->ssl, bio, bio);
  return &self->super.super;
}

static void
log_transport_tls_free_method(LogTransport *s)
{
  LogTransportTLS *self = (LogTransportTLS *) s;

  tls_session_free(self->tls_session);
  log_transport_adapter_free_method(s);
}

void
log_transport_tls_global_init(void)
{
  meth_transport = BIO_s_transport();
}

void
log_transport_tls_global_deinit(void)
{
  BIO_meth_free(meth_transport);
  meth_transport = NULL;
}
