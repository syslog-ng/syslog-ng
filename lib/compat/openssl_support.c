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
#include "syslog-ng.h"
#include "thread-utils.h"
#include <openssl/ssl.h>
#include <openssl/bn.h>

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

/* locking callbacks for OpenSSL prior to 1.1.0 */
#if OPENSSL_VERSION_NUMBER < 0x10100000L

static gint ssl_lock_count;
static GStaticMutex *ssl_locks;

static void
_ssl_locking_callback(int mode, int type, const char *file, int line)
{
  if (mode & CRYPTO_LOCK)
    {
      g_static_mutex_lock(&ssl_locks[type]);
    }
  else
    {
      g_static_mutex_unlock(&ssl_locks[type]);
    }
}

static void
_init_locks(void)
{
  gint i;

  ssl_lock_count = CRYPTO_num_locks();
  ssl_locks = g_new(GStaticMutex, ssl_lock_count);
  for (i = 0; i < ssl_lock_count; i++)
    {
      g_static_mutex_init(&ssl_locks[i]);
    }
  CRYPTO_set_locking_callback(_ssl_locking_callback);
}

static void
_deinit_locks(void)
{
  gint i;

  for (i = 0; i < ssl_lock_count; i++)
    {
      g_static_mutex_free(&ssl_locks[i]);
    }
  g_free(ssl_locks);
}

#else

static void
_init_locks(void)
{
}

static void
_deinit_locks(void)
{
}

#endif


/* ThreadID callbacks for various OpenSSL versions */
#if OPENSSL_VERSION_NUMBER < 0x10000000

static unsigned long
_ssl_thread_id(void)
{
  return (unsigned long) get_thread_id();
}

static void
_init_threadid_callback(void)
{
  CRYPTO_set_id_callback(_ssl_thread_id);
}

#else

static void
_ssl_thread_id2(CRYPTO_THREADID *id)
{
  CRYPTO_THREADID_set_numeric(id, (unsigned long) get_thread_id());
}

static void
_init_threadid_callback(void)
{
  CRYPTO_THREADID_set_callback(_ssl_thread_id2);
}

#endif

void
openssl_crypto_init_threading(void)
{
  _init_locks();
  _init_threadid_callback();
}

void
openssl_crypto_deinit_threading(void)
{
  _deinit_locks();
}

void
openssl_init(void)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();
#endif
}

void openssl_ctx_setup_ecdh(SSL_CTX *ctx)
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L

  /* No need to setup as ECDH auto is the default */

#elif OPENSSL_VERSION_NUMBER >= 0x10002000L

  SSL_CTX_set_ecdh_auto(ctx, 1);

#elif OPENSSL_VERSION_NUMBER >= 0x10001000L

  EC_KEY *ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if (!ecdh)
    return;

  SSL_CTX_set_tmp_ecdh(ctx, ecdh);
  EC_KEY_free(ecdh);

#endif
}

#if !SYSLOG_NG_HAVE_DECL_DH_SET0_PQG
int DH_set0_pqg(DH *dh, BIGNUM *p, BIGNUM *q, BIGNUM *g)
{
  if ((dh->p == NULL && p == NULL)
      || (dh->g == NULL && g == NULL))
    return 0;

  if (p != NULL)
    {
      BN_free(dh->p);
      dh->p = p;
    }
  if (q != NULL)
    {
      BN_free(dh->q);
      dh->q = q;
    }
  if (g != NULL)
    {
      BN_free(dh->g);
      dh->g = g;
    }

  if (q != NULL)
    dh->length = BN_num_bits(q);

  return 1;
}
#endif

#if !SYSLOG_NG_HAVE_DECL_BN_GET_RFC3526_PRIME_2048
BIGNUM *
BN_get_rfc3526_prime_2048(BIGNUM *bn)
{
  return get_rfc3526_prime_2048(bn);
}
#endif
