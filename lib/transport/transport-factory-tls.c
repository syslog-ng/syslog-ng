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

#include "transport/tls-context.h"
#include "transport/transport-factory-tls.h"
#include "transport/transport-tls.h"

static LogTransport *
_construct_transport(const LogTransportFactory *s, gint fd)
{
  LogTransportFactoryTLS *self = (LogTransportFactoryTLS *)s;
  TLSSession *tls_session = tls_context_setup_session(self->tls_context);

  if (!tls_session)
    return NULL;

  tls_session_configure_allow_compress(tls_session, self->allow_compress);

  tls_session_set_verifier(tls_session, self->tls_verifier);

  return log_transport_tls_new(tls_session, fd);
}

void
log_transport_factory_tls_enable_compression(LogTransportFactory *s)
{
  LogTransportFactoryTLS *self = (LogTransportFactoryTLS *)s;
  self->allow_compress = TRUE;
}

void
log_transport_factory_tls_disable_compression(LogTransportFactory *s)
{
  LogTransportFactoryTLS *self = (LogTransportFactoryTLS *)s;
  self->allow_compress = FALSE;
}

static void
_free(LogTransportFactory *s)
{
  LogTransportFactoryTLS *self = (LogTransportFactoryTLS *)s;
  tls_context_unref(self->tls_context);
  if (self->tls_verifier)
    tls_verifier_unref(self->tls_verifier);
}

LogTransportFactory *
log_transport_factory_tls_new(TLSContext *ctx, TLSVerifier *tls_verifier, guint32 flags)
{
  LogTransportFactoryTLS *instance = g_new0(LogTransportFactoryTLS, 1);

  instance->tls_context = tls_context_ref(ctx);
  instance->tls_verifier = tls_verifier ? tls_verifier_ref(tls_verifier) : NULL;

  instance->super.id = TRANSPORT_FACTORY_TLS_ID;
  instance->super.construct_transport = _construct_transport;
  instance->super.free_fn = _free;

  if (flags & TMI_ALLOW_COMPRESS)
    log_transport_factory_tls_enable_compression((LogTransportFactory *)instance);
  else
    log_transport_factory_tls_disable_compression((LogTransportFactory *)instance);

  return &instance->super;
}
