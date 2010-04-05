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

#ifndef TLSCONTEXT_H_INCLUDED
#define TLSCONTEXT_H_INCLUDED

#include "syslog-ng.h"

#if ENABLE_SSL

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

typedef gint (*TLSSessionVerifyFunc)(gint ok, X509_STORE_CTX *ctx, gpointer user_data);
typedef struct _TLSContext TLSContext;

typedef struct _TLSSession
{
  SSL *ssl;
  TLSContext *ctx;
  TLSSessionVerifyFunc verify_func;
  gpointer verify_data;
  GDestroyNotify verify_data_destroy;
} TLSSession;

void tls_session_set_verify(TLSSession *self, TLSSessionVerifyFunc verify_func, gpointer verify_data, GDestroyNotify verify_destroy);
void tls_session_free(TLSSession *self);

struct _TLSContext
{
  TLSMode mode;
  TLSVerifyMode verify_mode;
  gchar *key_file;
  gchar *cert_file;
  gchar *ca_dir;
  gchar *crl_dir;
  SSL_CTX *ssl_ctx;
  GList *trusted_fingerpint_list;
  GList *trusted_dn_list;
};


TLSSession *tls_context_setup_session(TLSContext *self);
void tls_session_set_trusted_fingerprints(TLSContext *self, GList *fingerprints);
void tls_session_set_trusted_dn(TLSContext *self, GList *dns);
TLSContext *tls_context_new(TLSMode mode);
void tls_context_free(TLSContext *s);

TLSVerifyMode tls_lookup_verify_mode(const gchar *mode_str);

void tls_log_certificate_validation_progress(int ok, X509_STORE_CTX *ctx);
gboolean tls_verify_certificate_name(X509 *cert, const gchar *hostname);

#else

typedef struct _TLSContext TLSContext;
typedef struct _TLSSession TLSSession;

#define tls_context_new(m)

#endif

#endif
