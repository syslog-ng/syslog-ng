/*
 * Copyright (c) 2002-2018 Balabit
 * Copyright (c) 2018 Laszlo Budai <laszlo.budai@balabit.com>
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

#include "transport/transport-factory-tls.h"
#include "transport/transport-tls.h"

DEFINE_TRANSPORT_FACTORY_ID_FUN("tls", transport_factory_tls_id);

typedef struct _TransportFactoryTLS TransportFactoryTLS;

struct _TransportFactoryTLS
{
  TransportFactory super;
  TLSContext *tls_context;
  TLSSessionVerifyFunc tls_verify_cb;
  gpointer tls_verify_data;
  gboolean allow_compress;
};

static void
_tls_session_allow_compress(TLSSession *tls_session, gboolean allow_compress)
{
  if (!allow_compress)
    {
      SSL_set_options(tls_session->ssl, SSL_OP_NO_COMPRESSION);
    }
  else
    {
      SSL_clear_options(tls_session->ssl, SSL_OP_NO_COMPRESSION);
    }
}

static LogTransport *
_construct_transport(const TransportFactory *s, gint fd)
{
  TransportFactoryTLS *self = (TransportFactoryTLS *)s;
  TLSSession *tls_session = tls_context_setup_session(self->tls_context);

  if (!tls_session)
    return NULL;

  _tls_session_allow_compress(tls_session, self->allow_compress);

  tls_session_set_verify(tls_session, self->tls_verify_cb, self->tls_verify_data, NULL);

  return log_transport_tls_new(tls_session, fd);
}

void
transport_factory_tls_enable_compression(TransportFactory *s)
{
  TransportFactoryTLS *self = (TransportFactoryTLS *)s;
  self->allow_compress = TRUE;
}

void
transport_factory_tls_disable_compression(TransportFactory *s)
{
  TransportFactoryTLS *self = (TransportFactoryTLS *)s;
  self->allow_compress = FALSE;
}

TransportFactory *
transport_factory_tls_new(TLSContext *ctx,
                          TLSSessionVerifyFunc tls_verify_cb,
                          gpointer tls_verify_data)
{
  TransportFactoryTLS *instance = g_new0(TransportFactoryTLS, 1);

  instance->tls_context = ctx;
  instance->tls_verify_cb = tls_verify_cb;
  instance->tls_verify_data  = tls_verify_data;

  instance->super.id = transport_factory_tls_id();
  instance->super.construct_transport = _construct_transport;

  return &instance->super;
}
