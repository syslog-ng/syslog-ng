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

#include "transport/tls-verifier.h"
#include "transport/tls-session.h"
#include "messages.h"

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
  TSO_NOTLSv13=0x0020,
  TSO_IGNORE_UNEXPECTED_EOF=0x0040,
  TSO_IGNORE_HOSTNAME_MISMATCH=0x0080,
  TSO_IGNORE_VALIDITY_PERIOD=0x0100,
} TLSSslOptions;

typedef enum
{
  TLS_CONTEXT_SETUP_OK,
  TLS_CONTEXT_SETUP_ERROR,
  TLS_CONTEXT_SETUP_BAD_PASSWORD
} TLSContextSetupResult;

struct _TLSContext
{
  GAtomicCounter ref_cnt;
  TLSMode mode;
  gint verify_mode;
  gchar *key_file;
  struct
  {
    gchar *keylog_file_path;
    FILE *keylog_file;
    GMutex keylog_file_lock;
  };
  gchar *cert_file;
  gchar *dhparam_file;
  gchar *pkcs12_file;
  gchar *ca_dir;
  gchar *crl_dir;
  gchar *ca_file;
  gchar *cipher_suite;
  gchar *tls13_cipher_suite;
  gchar *sigalgs;
  gchar *client_sigalgs;
  gchar *ecdh_curve_list;
  gchar *sni;
  gboolean ocsp_stapling_verify;

  SSL_CTX *ssl_ctx;
  GList *conf_cmds_list;
  GList *trusted_fingerprint_list;
  GList *trusted_dn_list;
  gint ssl_options;
  gint ssl_version;
  gchar *location;
};



#define TLSCONTEXT_ERROR tls_context_error_quark()
GQuark tls_context_error_quark(void);

enum TLSContextError
{
  TLSCONTEXT_UNSUPPORTED,
  TLSCONTEXT_INTERNAL_ERROR,
};

#define TMI_ALLOW_COMPRESS 0x1



gboolean tls_context_set_verify_mode_by_name(TLSContext *self, const gchar *mode_str);
gboolean tls_context_set_ssl_options_by_name(TLSContext *self, GList *options);
gboolean tls_context_set_ssl_version_by_name(TLSContext *self, const gchar *value);
gint tls_context_get_verify_mode(const TLSContext *self);
void tls_context_set_verify_mode(TLSContext *self, gint verify_mode);
gboolean tls_context_ignore_hostname_mismatch(TLSContext *self);
gboolean tls_context_ignore_validity_period(TLSContext *self);
void tls_context_set_key_file(TLSContext *self, const gchar *key_file);
void tls_context_set_cert_file(TLSContext *self, const gchar *cert_file);
gboolean tls_context_set_keylog_file(TLSContext *self, gchar *keylog_file_path, GError **error);
void tls_context_set_pkcs12_file(TLSContext *self, const gchar *pkcs12_file);
void tls_context_set_ca_dir(TLSContext *self, const gchar *ca_dir);
void tls_context_set_crl_dir(TLSContext *self, const gchar *crl_dir);
void tls_context_set_ca_file(TLSContext *self, const gchar *ca_file);
void tls_context_set_cipher_suite(TLSContext *self, const gchar *cipher_suite);
gboolean tls_context_set_tls13_cipher_suite(TLSContext *self, const gchar *tls13_cipher_suite, GError **error);
gboolean tls_context_set_sigalgs(TLSContext *self, const gchar *sigalgs, GError **error);
gboolean tls_context_set_client_sigalgs(TLSContext *self, const gchar *sigalgs, GError **error);
gboolean tls_context_set_conf_cmds(TLSContext *self, GList *cmds, GError **error);
void tls_context_set_ecdh_curve_list(TLSContext *self, const gchar *ecdh_curve_list);
void tls_context_set_dhparam_file(TLSContext *self, const gchar *dhparam_file);
void tls_context_set_sni(TLSContext *self, const gchar *sni);
void tls_context_set_ocsp_stapling_verify(TLSContext *self, gboolean ocsp_stapling_verify);
const gchar *tls_context_get_key_file(TLSContext *self);
EVTTAG *tls_context_format_tls_error_tag(TLSContext *self);
EVTTAG *tls_context_format_location_tag(TLSContext *self);
gboolean tls_context_verify_peer(TLSContext *self, X509 *peer_cert, const gchar *peer_name);
TLSContextSetupResult tls_context_setup_context(TLSContext *self);
TLSSession *tls_context_setup_session(TLSContext *self);
TLSContext *tls_context_new(TLSMode mode, const gchar *config_location);
TLSContext *tls_context_ref(TLSContext *self);
void tls_context_unref(TLSContext *self);

void tls_x509_format_dn(X509_NAME *name, GString *dn);

#endif
