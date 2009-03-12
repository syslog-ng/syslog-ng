/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "tlstransport.h"

#if ENABLE_SSL

#include "messages.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <errno.h>

typedef struct _LogTransportTLS
{
  LogTransport super;
  TLSSession *tls_session;
} LogTransportTLS;

static gssize
log_transport_tls_read_method(LogTransport *s, gpointer buf, gsize buflen, GSockAddr **sa)
{
  LogTransportTLS *self = (LogTransportTLS *) s;
  gint ssl_error;
  gint rc;
  
  if (sa)
    *sa = NULL;

  /* assume that we need to poll our input for reading unless
   * SSL_ERROR_WANT_WRITE is specified by libssl */
  self->super.cond = G_IO_IN;
  
  do
    {
      rc = SSL_read(self->tls_session->ssl, buf, buflen);

      if (rc < 0)
        {
          ssl_error = SSL_get_error(self->tls_session->ssl, rc);
          switch (ssl_error)
            {
            case SSL_ERROR_WANT_READ:
              errno = EAGAIN;
              break;
            case SSL_ERROR_WANT_WRITE:
              /* although we are writing this fd, libssl wants to write. This
               * happens during renegotiation for example */
              self->super.cond = G_IO_OUT;
              errno = EAGAIN;
              break;
            case SSL_ERROR_SYSCALL:
              /* errno is set accordingly */
              break;
            default:
              goto tls_error;
            }
        }
    }
  while (rc == -1 && errno == EINTR);
  
  return rc;
 tls_error:

  ssl_error = ERR_get_error();
  msg_error("SSL error while reading stream", 
            evt_tag_printf("tls_error", "%s:%s:%s", ERR_lib_error_string(ssl_error), ERR_func_error_string(ssl_error), ERR_reason_error_string(ssl_error)),
            NULL);
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
  
  self->super.cond = G_IO_OUT;

  rc = SSL_write(self->tls_session->ssl, buf, buflen);
    
  if (rc < 0)
    {
      ssl_error = SSL_get_error(self->tls_session->ssl, rc);
      switch (ssl_error)
        {
        case SSL_ERROR_WANT_READ:
          /* although we are writing this fd, libssl wants to read. This
           * happens during renegotiation for example */
          self->super.cond = G_IO_IN;
          errno = EAGAIN;
          break;
        case SSL_ERROR_WANT_WRITE:
          errno = EAGAIN;
          break;
        case SSL_ERROR_SYSCALL:
          /* errno is set accordingly */
          break;
        default:
          goto tls_error;
        }
    }

  return rc;

 tls_error:
 
  ssl_error = ERR_get_error();
  msg_error("SSL error while writing stream", 
            evt_tag_printf("tls_error", "%s:%s:%s", ERR_lib_error_string(ssl_error), ERR_func_error_string(ssl_error), ERR_reason_error_string(ssl_error)),
            NULL);
  ERR_clear_error();
 
  errno = EPIPE;
  return -1;
}


static void log_transport_tls_free_method(LogTransport *s);

LogTransport *
log_transport_tls_new(TLSSession *tls_session, gint fd, guint flags)
{
  LogTransportTLS *self = g_new0(LogTransportTLS, 1);
  
  self->super.flags = flags;
  self->super.fd = fd;
  self->super.cond = G_IO_IN | G_IO_OUT;
  self->super.read = log_transport_tls_read_method;
  self->super.write = log_transport_tls_write_method;
  self->super.free_fn = log_transport_tls_free_method;
  self->tls_session = tls_session;
  
  SSL_set_fd(self->tls_session->ssl, fd);
  
  g_assert((self->super.flags & LTF_RECV) == 0);
  return &self->super;
}

static void
log_transport_tls_free_method(LogTransport *s)
{
  LogTransportTLS *self = (LogTransportTLS *) s;
  
  tls_session_free(self->tls_session);
  log_transport_free_method(s);
}

#endif
