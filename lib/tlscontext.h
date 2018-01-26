/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#ifndef TLSCONTEXT_H_INCLUDED
#define TLSCONTEXT_H_INCLUDED

#include "syslog-ng.h"

#include <openssl/ssl.h>

typedef enum
{
  TM_CLIENT,
  TM_SERVER,
  TM_MAX
} TLSMode;

typedef enum
{
  TVM_NONE,
  TVM_TRUSTED=0x0001,
  TVM_UNTRUSTED=0x0002,
  TVM_OPTIONAL=0x0010,
  TVM_REQUIRED=0x0020,
} TLSVerifyMode;

typedef enum
{
  TSO_NONE,
  TSO_NOSSLv2=0x0001,
  TSO_NOSSLv3=0x0002,
  TSO_NOTLSv1=0x0004,
  TSO_NOTLSv11=0x0008,
  TSO_NOTLSv12=0x0010,
} TLSSslOptions;

typedef enum
{
  TLS_CONTEXT_SETUP_OK,
  TLS_CONTEXT_SETUP_ERROR,
  TLS_CONTEXT_SETUP_BAD_PASSWORD
} TLSContextSetupResult;

typedef gint (*TLSSessionVerifyFunc)(gint ok, X509_STORE_CTX *ctx, gpointer user_data);
typedef struct _TLSContext TLSContext;

#define X509_MAX_CN_LEN 64
#define X509_MAX_O_LEN 64
#define X509_MAX_OU_LEN 32

typedef struct _TLSSession
{
  SSL *ssl;
  TLSContext *ctx;
  TLSSessionVerifyFunc verify_func;
  gpointer verify_data;
  GDestroyNotify verify_data_destroy;
  struct
  {
    int found;
    gchar o[X509_MAX_O_LEN];
    gchar ou[X509_MAX_OU_LEN];
    gchar cn[X509_MAX_CN_LEN];
  } peer_info;
} TLSSession;

void tls_session_set_verify(TLSSession *self, TLSSessionVerifyFunc verify_func, gpointer verify_data,
                            GDestroyNotify verify_destroy);
void tls_session_free(TLSSession *self);

TLSContextSetupResult tls_context_setup_context(TLSContext *self);
TLSSession *tls_context_setup_session(TLSContext *self);
void tls_session_set_trusted_fingerprints(TLSContext *self, GList *fingerprints);
void tls_session_set_trusted_dn(TLSContext *self, GList *dns);

TLSContext *tls_context_new(TLSMode mode);
void tls_context_free(TLSContext *s);

gboolean tls_context_set_verify_mode_by_name(TLSContext *self, const gchar *mode_str);
gboolean tls_context_set_ssl_options_by_name(TLSContext *self, GList *options);
gint tls_context_get_verify_mode(const TLSContext *self);
void tls_context_set_verify_mode(TLSContext *self, gint verify_mode);
void tls_context_set_key_file(TLSContext *self, const gchar *key_file);
void tls_context_set_cert_file(TLSContext *self, const gchar *cert_file);
void tls_context_set_pkcs12_file(TLSContext *self, const gchar *pkcs12_file);
void tls_context_set_ca_dir(TLSContext *self, const gchar *ca_dir);
void tls_context_set_crl_dir(TLSContext *self, const gchar *crl_dir);
void tls_context_set_cipher_suite(TLSContext *self, const gchar *cipher_suite);
void tls_context_set_ecdh_curve_list(TLSContext *self, const gchar *ecdh_curve_list);
void tls_context_set_dhparam_file(TLSContext *self, const gchar *dhparam_file);

void tls_log_certificate_validation_progress(int ok, X509_STORE_CTX *ctx);
gboolean tls_verify_certificate_name(X509 *cert, const gchar *hostname);

void tls_x509_format_dn(X509_NAME *name, GString *dn);

#endif
