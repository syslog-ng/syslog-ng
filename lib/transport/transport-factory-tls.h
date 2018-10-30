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

#ifndef TRANSPORT_FACTORY_TLS_H_INCLUDED
#define TRANSPORT_FACTORY_TLS_H_INCLUDED

#include "transport/transport-factory.h"
#include "tlscontext.h"

typedef struct _TransportFactoryTLS TransportFactoryTLS;

struct _TransportFactoryTLS
{
  TransportFactory super;
  TLSContext *tls_context;
  TLSVerifier *tls_verifier;
  gboolean allow_compress;
};

TransportFactory *transport_factory_tls_new(TLSContext *ctx, TLSVerifier *tls_verifier, gboolean allow_compress);

void transport_factory_tls_enable_compression(TransportFactory *);
void transport_factory_tls_disable_compression(TransportFactory *);

const TransportFactoryId *transport_factory_tls_id(void);

#endif
