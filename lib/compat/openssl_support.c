/*
 * Copyright (c) 2002-2016 Balabit
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

#include "compat/openssl_support.h"

#if !SYSLOG_NG_HAVE_DECL_SSL_CTX_GET0_PARAM
X509_VERIFY_PARAM *SSL_CTX_get0_param(SSL_CTX *ctx)
{
  return ctx->param;
}
#endif

#if !SYSLOG_NG_HAVE_DECL_X509_STORE_CTX_GET0_CERT
X509 *X509_STORE_CTX_get0_cert(X509_STORE_CTX *ctx)
{
  return ctx->cert;
}
#endif

#if !SYSLOG_NG_HAVE_DECL_X509_GET_EXTENSION_FLAGS
uint32_t X509_get_extension_flags(X509 *x)
{
  return x->ex_flags;
}
#endif

