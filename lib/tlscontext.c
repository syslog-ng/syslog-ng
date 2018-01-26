/*
 * Copyright (c) 2002-2011 Balabit
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

#include "tlscontext.h"
#include "str-utils.h"
#include "messages.h"
#include "compat/openssl_support.h"

#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/dh.h>
#include <openssl/bn.h>
#include <openssl/pkcs12.h>

struct _TLSContext
{
  TLSMode mode;
  gint verify_mode;
  gchar *key_file;
  gchar *cert_file;
  gchar *dhparam_file;
  gchar *pkcs12_file;
  gchar *ca_dir;
  gchar *crl_dir;
  gchar *cipher_suite;
  gchar *ecdh_curve_list;
  SSL_CTX *ssl_ctx;
  GList *trusted_fingerprint_list;
  GList *trusted_dn_list;
  gint ssl_options;
};

typedef enum
{
  TLS_CONTEXT_OK,
  TLS_CONTEXT_ERROR,
  TLS_CONTEXT_FILE_ACCES_ERROR,
  TLS_CONTEXT_PASSWORD_ERROR
} TLSContextLoadResult;

gboolean
tls_get_x509_digest(X509 *x, GString *hash_string)
{
  gint j;
  unsigned int n;
  unsigned char md[EVP_MAX_MD_SIZE];
  g_assert(hash_string);

  if (!X509_digest(x, EVP_sha1(), md, &n))
    return FALSE;

  g_string_append(hash_string, "SHA1:");
  for (j = 0; j < (int) n; j++)
    g_string_append_printf(hash_string, "%02X%c", md[j], (j + 1 == (int) n) ?'\0' : ':');

  return TRUE;
}

int
tls_session_verify_fingerprint(X509_STORE_CTX *ctx)
{
  SSL *ssl = (SSL *)X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
  TLSSession *self = SSL_get_app_data(ssl);
  GList *current_fingerprint = self->ctx->trusted_fingerprint_list;
  GString *hash;
  gboolean match = FALSE;
  X509 *cert = X509_STORE_CTX_get_current_cert(ctx);

  if (!current_fingerprint)
    {
      return TRUE;
    }

  if (!cert)
    return match;

  hash = g_string_sized_new(EVP_MAX_MD_SIZE * 3);

  if (tls_get_x509_digest(cert, hash))
    {
      do
        {
          if (strcmp((const gchar *)(current_fingerprint->data), hash->str) == 0)
            {
              match = TRUE;
              break;
            }
        }
      while ((current_fingerprint = g_list_next(current_fingerprint)) != NULL);
    }

  g_string_free(hash, TRUE);
  return match;
}

void
tls_x509_format_dn(X509_NAME *name, GString *dn)
{
  BIO *bio;
  gchar *buf;
  long len;

  bio = BIO_new(BIO_s_mem());
  X509_NAME_print_ex(bio, name, 0, ASN1_STRFLGS_ESC_2253 | ASN1_STRFLGS_UTF8_CONVERT | XN_FLAG_SEP_CPLUS_SPC |
                     XN_FLAG_DN_REV);
  len = BIO_get_mem_data(bio, &buf);
  g_string_assign_len(dn, buf, len);
  BIO_free(bio);
}

int
tls_session_verify_dn(X509_STORE_CTX *ctx)
{
  SSL *ssl = (SSL *)X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
  TLSSession *self = SSL_get_app_data(ssl);
  gboolean match = FALSE;
  GList *current_dn = self->ctx->trusted_dn_list;
  X509 *cert = X509_STORE_CTX_get_current_cert(ctx);
  GString *dn;

  if (!current_dn || !cert)
    return TRUE;

  dn = g_string_sized_new(128);
  tls_x509_format_dn(X509_get_subject_name(cert), dn);

  do
    {
      if (g_pattern_match_simple((const gchar *) current_dn->data, dn->str))
        {
          match = TRUE;
          break;
        }
    }
  while ((current_dn = g_list_next(current_dn)) != NULL);
  return match;
}

int
tls_session_verify(TLSSession *self, int ok, X509_STORE_CTX *ctx)
{
  /* untrusted means that we have to accept the certificate even if it is untrusted */
  if (self->ctx->verify_mode & TVM_UNTRUSTED)
    return 1;

  int ctx_error_depth = X509_STORE_CTX_get_error_depth(ctx);
  /* accept certificate if its fingerprint matches, again regardless whether x509 certificate validation was successful */
  if (ok && ctx_error_depth == 0 && !tls_session_verify_fingerprint(ctx))
    {
      msg_notice("Certificate valid, but fingerprint constraints were not met, rejecting");
      return 0;
    }

  X509 *current_cert = X509_STORE_CTX_get_current_cert(ctx);
  if (ok && ctx_error_depth != 0 && (X509_get_extension_flags(current_cert) & EXFLAG_CA) == 0)
    {
      msg_notice("Invalid certificate found in chain, basicConstraints.ca is unset in non-leaf certificate");
      X509_STORE_CTX_set_error(ctx, X509_V_ERR_INVALID_CA);
      return 0;
    }

  /* reject certificate if it is valid, but its DN is not trusted */
  if (ok && ctx_error_depth == 0 && !tls_session_verify_dn(ctx))
    {
      msg_notice("Certificate valid, but DN constraints were not met, rejecting");
      X509_STORE_CTX_set_error(ctx, X509_V_ERR_CERT_UNTRUSTED);
      return 0;
    }
  /* if the crl_dir is set in the configuration file but the directory is empty ignore this error */
  if (!ok && X509_STORE_CTX_get_error(ctx) == X509_V_ERR_UNABLE_TO_GET_CRL)
    {
      msg_notice("CRL directory is set but no CRLs found");
      return 1;
    }

  if (!ok && X509_STORE_CTX_get_error(ctx) == X509_V_ERR_INVALID_PURPOSE)
    {
      msg_warning("Certificate valid, but purpose is invalid");
      return 1;
    }
  return ok;
}

int
tls_session_verify_callback(int ok, X509_STORE_CTX *ctx)
{
  SSL *ssl = (SSL *)X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
  TLSSession *self = SSL_get_app_data(ssl);
  /* NOTE: Sometimes libssl calls this function
     with no current_cert. This happens when
     some global error is happen. At this situation
     we do not need to call any other check and callback
   */
  if (X509_STORE_CTX_get_current_cert(ctx) == NULL)
    {
      int ctx_error = X509_STORE_CTX_get_error(ctx);
      switch (ctx_error)
        {
        case X509_V_ERR_NO_EXPLICIT_POLICY:
          /* NOTE: Because we set the CHECK_POLICY_FLAG if the
             certificate contains ExplicitPolicy constraint
             we would get this error. But this error is because
             we do not set the policy what we want to check for.
           */
          ok = 1;
          break;
        default:
          msg_notice("Error occured during certificate validation",
                     evt_tag_int("error", X509_STORE_CTX_get_error(ctx)));
          break;
        }
    }
  else
    {
      ok = tls_session_verify(self, ok, ctx);

      tls_log_certificate_validation_progress(ok, ctx);

      if (self->verify_func)
        return self->verify_func(ok, ctx, self->verify_data);
    }
  return ok;
}

void
tls_session_set_trusted_fingerprints(TLSContext *self, GList *fingerprints)
{
  g_assert(fingerprints);

  self->trusted_fingerprint_list = fingerprints;
}

void
tls_session_set_trusted_dn(TLSContext *self, GList *dn)
{
  g_assert(dn);

  self->trusted_dn_list = dn;
}

void
tls_session_set_verify(TLSSession *self, TLSSessionVerifyFunc verify_func, gpointer verify_data,
                       GDestroyNotify verify_destroy)
{
  self->verify_func = verify_func;
  self->verify_data = verify_data;
  self->verify_data_destroy = verify_destroy;
}

void
tls_session_info_callback(const SSL *ssl, int where, int ret)
{
  TLSSession *self = (TLSSession *)SSL_get_app_data(ssl);
  if( !self->peer_info.found && where == (SSL_ST_ACCEPT|SSL_CB_LOOP) )
    {
      X509 *cert = SSL_get_peer_certificate(ssl);

      if(cert)
        {
          self->peer_info.found = 1; /* mark this found so we don't keep checking on every callback */
          X509_NAME *name = X509_get_subject_name(cert);

          X509_NAME_get_text_by_NID( name, NID_commonName, self->peer_info.cn, X509_MAX_CN_LEN );
          X509_NAME_get_text_by_NID( name, NID_organizationName, self->peer_info.o, X509_MAX_O_LEN );
          X509_NAME_get_text_by_NID( name, NID_organizationalUnitName, self->peer_info.ou, X509_MAX_OU_LEN );

          X509_free(cert);
        }
    }
}

static TLSSession *
tls_session_new(SSL *ssl, TLSContext *ctx)
{
  TLSSession *self = g_new0(TLSSession, 1);

  self->ssl = ssl;
  self->ctx = ctx;

  /* to set verify callback */
  tls_session_set_verify(self, NULL, NULL, NULL);

  SSL_set_info_callback(ssl, tls_session_info_callback);

  return self;
}

void
tls_session_free(TLSSession *self)
{
  if (self->verify_data && self->verify_data_destroy)
    self->verify_data_destroy(self->verify_data);
  SSL_free(self->ssl);

  g_free(self);
}

static gboolean
file_exists(const gchar *fname)
{
  if (!fname)
    return FALSE;
  if (access(fname, R_OK) < 0)
    {
      msg_error("Error opening TLS file",
                evt_tag_str("filename", fname),
                evt_tag_errno("error", errno));
      return FALSE;
    }
  return TRUE;
}

static void
_print_and_clear_tls_session_error(void)
{
  gulong ssl_error = ERR_get_error();
  msg_error("Error setting up TLS session context",
            evt_tag_printf("tls_error", "%s:%s:%s",
                           ERR_lib_error_string(ssl_error),
                           ERR_func_error_string(ssl_error),
                           ERR_reason_error_string(ssl_error)));
  ERR_clear_error();
}

static void
tls_context_setup_verify_mode(TLSContext *self)
{
  gint verify_mode = 0;

  switch (self->verify_mode)
    {
    case TVM_NONE:
      verify_mode = SSL_VERIFY_NONE;
      break;
    case TVM_OPTIONAL | TVM_UNTRUSTED:
      verify_mode = SSL_VERIFY_NONE;
      break;
    case TVM_OPTIONAL | TVM_TRUSTED:
      verify_mode = SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE;
      break;
    case TVM_REQUIRED | TVM_UNTRUSTED:
      verify_mode = SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
      break;
    case TVM_REQUIRED | TVM_TRUSTED:
      verify_mode = SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
      break;
    default:
      g_assert_not_reached();
    }

  SSL_CTX_set_verify(self->ssl_ctx, verify_mode, tls_session_verify_callback);
}

static void
tls_context_setup_ssl_options(TLSContext *self)
{
  if (self->ssl_options != TSO_NONE)
    {
      glong ssl_options = 0;
      if(self->ssl_options & TSO_NOSSLv2)
        ssl_options |= SSL_OP_NO_SSLv2;
      if(self->ssl_options & TSO_NOSSLv3)
        ssl_options |= SSL_OP_NO_SSLv3;
      if(self->ssl_options & TSO_NOTLSv1)
        ssl_options |= SSL_OP_NO_TLSv1;
#ifdef SSL_OP_NO_TLSv1_2
      if(self->ssl_options & TSO_NOTLSv11)
        ssl_options |= SSL_OP_NO_TLSv1_1;
      if(self->ssl_options & TSO_NOTLSv12)
        ssl_options |= SSL_OP_NO_TLSv1_2;
#endif
#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE
      if (self->mode == TM_SERVER)
        ssl_options |= SSL_OP_CIPHER_SERVER_PREFERENCE;
#endif
      SSL_CTX_set_options(self->ssl_ctx, ssl_options);
    }
  else
    {
      msg_debug("empty ssl options");
    }
}

static gboolean
_set_optional_ecdh_curve_list(SSL_CTX *ctx, const gchar *ecdh_curve_list)
{
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
  if (ecdh_curve_list && !SSL_CTX_set1_curves_list(ctx, ecdh_curve_list))
    {
      msg_error("Error setting up TLS session context, invalid curve name in list",
                evt_tag_str("ecdh_curve_list", ecdh_curve_list));
      return FALSE;
    }
#endif

  return TRUE;
}

static gboolean
_is_dh_valid(DH *dh)
{
  if (!dh)
    return FALSE;

  gint check_flags;
  if (!DH_check(dh, &check_flags))
    return FALSE;

  gboolean error_flag_is_set = check_flags &
                               (DH_CHECK_P_NOT_PRIME
                                | DH_UNABLE_TO_CHECK_GENERATOR
                                | DH_CHECK_P_NOT_SAFE_PRIME
                                | DH_NOT_SUITABLE_GENERATOR);

  return !error_flag_is_set;
}

static DH *
_load_dh_from_file(const gchar *dhparam_file)
{
  if (!file_exists(dhparam_file))
    return NULL;

  BIO *bio = BIO_new_file(dhparam_file, "r");
  if (!bio)
    return NULL;

  DH *dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
  BIO_free(bio);

  if (!_is_dh_valid(dh))
    {
      msg_error("Error setting up TLS session context, invalid DH parameters",
                evt_tag_str("dhparam_file", dhparam_file),
                NULL);

      DH_free(dh);
      return NULL;
    }

  return dh;
}

static DH *
_load_dh_fallback(void)
{
  DH *dh = DH_new();

  if (!dh)
    return NULL;

  /*
   * "2048-bit MODP Group" from RFC3526, Section 3.
   *
   * The prime is: 2^2048 - 2^1984 - 1 + 2^64 * { [2^1918 pi] + 124476 }
   *
   * RFC3526 specifies a generator of 2.
   */

  BIGNUM *g = NULL;
  BN_dec2bn(&g, "2");

  if (!DH_set0_pqg(dh, get_rfc3526_prime_2048(NULL), NULL, g))
    {
      BN_free(g);
      DH_free(dh);
      return NULL;
    }

  return dh;
}

static gboolean
tls_context_setup_ecdh(TLSContext *self)
{
  /* server only */
  if (self->mode != TM_SERVER)
    return TRUE;

  if (!_set_optional_ecdh_curve_list(self->ssl_ctx, self->ecdh_curve_list))
    return FALSE;

  openssl_ctx_setup_ecdh(self->ssl_ctx);

  return TRUE;
}

static gboolean
tls_context_setup_dh(TLSContext *self)
{
  DH *dh = self->dhparam_file ? _load_dh_from_file(self->dhparam_file) : _load_dh_fallback();

  if (!dh)
    return FALSE;

  gboolean ctx_dh_success = SSL_CTX_set_tmp_dh(self->ssl_ctx, dh);

  DH_free(dh);
  return ctx_dh_success;
}

static PKCS12 *
_load_pkcs12_file(const gchar *pkcs12_file)
{
  if (!file_exists(pkcs12_file))
    return NULL;

  FILE *p12_file = fopen(pkcs12_file, "rb");

  if (!p12_file)
    return NULL;

  PKCS12 *pkcs12 = d2i_PKCS12_fp(p12_file, NULL);
  fclose(p12_file);

  return pkcs12;
}

static gboolean
_extract_pkcs12_content(PKCS12 *pkcs12, EVP_PKEY **private_key, X509 **cert, STACK_OF(X509) **ca_list)
{
  return PKCS12_parse(pkcs12, "", private_key, cert, ca_list);
}

static gboolean
tls_context_add_certs(TLSContext *self, STACK_OF(X509) *ca_list)
{
  for (gint ca_index = 0; ca_index < sk_X509_num(ca_list); ++ca_index)
    {
      X509 *current_ca = sk_X509_value(ca_list, ca_index);

      if (!X509_STORE_add_cert(SSL_CTX_get_cert_store(self->ssl_ctx), current_ca))
        return FALSE;
      if (self->mode == TM_SERVER && !SSL_CTX_add_client_CA(self->ssl_ctx, current_ca))
        return FALSE;
    }

  return TRUE;
}

static gboolean
tls_context_load_pkcs12(TLSContext *self)
{
  PKCS12 *pkcs12 = _load_pkcs12_file(self->pkcs12_file);

  if (!pkcs12)
    return FALSE;

  EVP_PKEY *private_key;
  X509 *cert;
  STACK_OF(X509) *ca_list = NULL;

  if (!_extract_pkcs12_content(pkcs12, &private_key, &cert, &ca_list))
    {
      PKCS12_free(pkcs12);
      return FALSE;
    }

  PKCS12_free(pkcs12);

  if (ca_list && !tls_context_add_certs(self, ca_list))
    return FALSE;

  return SSL_CTX_use_certificate(self->ssl_ctx, cert)
         && SSL_CTX_use_PrivateKey(self->ssl_ctx, private_key)
         && SSL_CTX_check_private_key(self->ssl_ctx);
}

static gboolean
_are_key_and_cert_files_accessible(TLSContext *self)
{
  return file_exists(self->key_file) &&
         file_exists(self->cert_file);
}

static TLSContextLoadResult
tls_context_load_key_and_cert(TLSContext *self)
{
  if (!_are_key_and_cert_files_accessible(self))
    return TLS_CONTEXT_FILE_ACCES_ERROR;
  if (!SSL_CTX_use_PrivateKey_file(self->ssl_ctx, self->key_file, SSL_FILETYPE_PEM))
    return TLS_CONTEXT_PASSWORD_ERROR;
  if (!SSL_CTX_use_certificate_chain_file(self->ssl_ctx, self->cert_file))
    return TLS_CONTEXT_ERROR;
  if (self->cert_file && !SSL_CTX_check_private_key(self->ssl_ctx))
    return TLS_CONTEXT_PASSWORD_ERROR;

  return TLS_CONTEXT_OK;
}

TLSContextSetupResult
tls_context_setup_context(TLSContext *self)
{
  gint verify_flags = X509_V_FLAG_POLICY_CHECK;

  if (!self->ssl_ctx)
    goto error;

  if (self->pkcs12_file)
    {
      if (self->cert_file || self->key_file)
        msg_warning("WARNING: pkcs12-file() is specified, key-file() and cert-file() will be omitted");

      if (!tls_context_load_pkcs12(self))
        goto error;
    }
  else
    {
      TLSContextLoadResult r = tls_context_load_key_and_cert(self);
      if (r == TLS_CONTEXT_PASSWORD_ERROR)
        goto password_error;
      if (r != TLS_CONTEXT_OK)
        goto error;
    }

  if (file_exists(self->ca_dir) && !SSL_CTX_load_verify_locations(self->ssl_ctx, NULL, self->ca_dir))
    goto error;

  if (file_exists(self->crl_dir) && !SSL_CTX_load_verify_locations(self->ssl_ctx, NULL, self->crl_dir))
    goto error;

  if (self->crl_dir)
    verify_flags |= X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL;

  X509_VERIFY_PARAM_set_flags(SSL_CTX_get0_param(self->ssl_ctx), verify_flags);

  tls_context_setup_verify_mode(self);
  tls_context_setup_ssl_options(self);
  if (!tls_context_setup_ecdh(self))
    {
      SSL_CTX_free(self->ssl_ctx);
      self->ssl_ctx = NULL;
      return TLS_CONTEXT_SETUP_ERROR;
    }

  if (!tls_context_setup_dh(self))
    goto error;

  if (self->cipher_suite)
    {
      if (!SSL_CTX_set_cipher_list(self->ssl_ctx, self->cipher_suite))
        goto error;
    }

  return TLS_CONTEXT_SETUP_OK;

error:
  _print_and_clear_tls_session_error();
  if (self->ssl_ctx)
    {
      SSL_CTX_free(self->ssl_ctx);
      self->ssl_ctx = NULL;
    }
  return TLS_CONTEXT_SETUP_ERROR;
password_error:
  _print_and_clear_tls_session_error();
  return TLS_CONTEXT_SETUP_BAD_PASSWORD;
}

TLSSession *
tls_context_setup_session(TLSContext *self)
{
  if (!self->ssl_ctx)
    return NULL;

  SSL *ssl = SSL_new(self->ssl_ctx);

  if (self->mode == TM_CLIENT)
    SSL_set_connect_state(ssl);
  else
    SSL_set_accept_state(ssl);

  TLSSession *session = tls_session_new(ssl, self);
  SSL_set_app_data(ssl, session);
  return session;
}

TLSContext *
tls_context_new(TLSMode mode)
{
  TLSContext *self = g_new0(TLSContext, 1);

  self->mode = mode;
  self->verify_mode = TVM_REQUIRED | TVM_TRUSTED;
  self->ssl_options = TSO_NOSSLv2;

  if (self->mode == TM_CLIENT)
    self->ssl_ctx = SSL_CTX_new(SSLv23_client_method());
  else
    self->ssl_ctx = SSL_CTX_new(SSLv23_server_method());

  return self;
}

void
tls_context_free(TLSContext *self)
{
  SSL_CTX_free(self->ssl_ctx);
  g_list_foreach(self->trusted_fingerprint_list, (GFunc) g_free, NULL);
  g_list_foreach(self->trusted_dn_list, (GFunc) g_free, NULL);
  g_free(self->key_file);
  g_free(self->pkcs12_file);
  g_free(self->cert_file);
  g_free(self->dhparam_file);
  g_free(self->ca_dir);
  g_free(self->crl_dir);
  g_free(self->cipher_suite);
  g_free(self->ecdh_curve_list);
  g_free(self);
}

gboolean
tls_context_set_verify_mode_by_name(TLSContext *self, const gchar *mode_str)
{
  if (strcasecmp(mode_str, "none") == 0)
    self->verify_mode = TVM_NONE;
  else if (strcasecmp(mode_str, "optional-trusted") == 0 || strcasecmp(mode_str, "optional_trusted") == 0)
    self->verify_mode = TVM_OPTIONAL | TVM_TRUSTED;
  else if (strcasecmp(mode_str, "optional-untrusted") == 0 || strcasecmp(mode_str, "optional_untrusted") == 0)
    self->verify_mode = TVM_OPTIONAL | TVM_UNTRUSTED;
  else if (strcasecmp(mode_str, "required-trusted") == 0 || strcasecmp(mode_str, "required_trusted") == 0)
    self->verify_mode = TVM_REQUIRED | TVM_TRUSTED;
  else if (strcasecmp(mode_str, "required-untrusted") == 0 || strcasecmp(mode_str, "required_untrusted") == 0)
    self->verify_mode = TVM_REQUIRED | TVM_UNTRUSTED;
  else
    return FALSE;

  return TRUE;
}

gboolean
tls_context_set_ssl_options_by_name(TLSContext *self, GList *options)
{
  self->ssl_options = TSO_NONE;

  GList *l;
  for (l=options; l != NULL; l=l->next)
    {
      if (strcasecmp(l->data, "no-sslv2") == 0 || strcasecmp(l->data, "no_sslv2") == 0)
        self->ssl_options |= TSO_NOSSLv2;
      else if (strcasecmp(l->data, "no-sslv3") == 0 || strcasecmp(l->data, "no_sslv3") == 0)
        self->ssl_options |= TSO_NOSSLv3;
      else if (strcasecmp(l->data, "no-tlsv1") == 0 || strcasecmp(l->data, "no_tlsv1") == 0)
        self->ssl_options |= TSO_NOTLSv1;
#ifdef SSL_OP_NO_TLSv1_2
      else if (strcasecmp(l->data, "no-tlsv11") == 0 || strcasecmp(l->data, "no_tlsv11") == 0)
        self->ssl_options |= TSO_NOTLSv11;
      else if (strcasecmp(l->data, "no-tlsv12") == 0 || strcasecmp(l->data, "no_tlsv12") == 0)
        self->ssl_options |= TSO_NOTLSv12;
#endif
      else
        return FALSE;
    }

  return TRUE;
}

gint
tls_context_get_verify_mode(const TLSContext *self)
{
  return self->verify_mode;
}

void tls_context_set_verify_mode(TLSContext *self, gint verify_mode)
{
  self->verify_mode = verify_mode;
}

void
tls_context_set_key_file(TLSContext *self, const gchar *key_file)
{
  g_free(self->key_file);
  self->key_file = g_strdup(key_file);
}

void
tls_context_set_cert_file(TLSContext *self, const gchar *cert_file)
{
  g_free(self->cert_file);
  self->cert_file = g_strdup(cert_file);
}

void
tls_context_set_pkcs12_file(TLSContext *self, const gchar *pkcs12_file)
{
  g_free(self->pkcs12_file);
  self->pkcs12_file = g_strdup(pkcs12_file);
}

void
tls_context_set_ca_dir(TLSContext *self, const gchar *ca_dir)
{
  g_free(self->ca_dir);
  self->ca_dir = g_strdup(ca_dir);
}

void
tls_context_set_crl_dir(TLSContext *self, const gchar *crl_dir)
{
  g_free(self->crl_dir);
  self->crl_dir = g_strdup(crl_dir);
}

void
tls_context_set_cipher_suite(TLSContext *self, const gchar *cipher_suite)
{
  g_free(self->cipher_suite);
  self->cipher_suite = g_strdup(cipher_suite);
}

void
tls_context_set_ecdh_curve_list(TLSContext *self, const gchar *ecdh_curve_list)
{
  g_free(self->ecdh_curve_list);
  self->ecdh_curve_list = g_strdup(ecdh_curve_list);
}

void
tls_context_set_dhparam_file(TLSContext *self, const gchar *dhparam_file)
{
  g_free(self->dhparam_file);
  self->dhparam_file = g_strdup(dhparam_file);
}

void
tls_log_certificate_validation_progress(int ok, X509_STORE_CTX *ctx)
{
  X509 *xs;
  GString *subject_name, *issuer_name;

  xs = X509_STORE_CTX_get_current_cert(ctx);

  subject_name = g_string_sized_new(128);
  issuer_name = g_string_sized_new(128);
  tls_x509_format_dn(X509_get_subject_name(xs), subject_name);
  tls_x509_format_dn(X509_get_issuer_name(xs), issuer_name);

  if (ok)
    {
      msg_debug("Certificate validation progress",
                evt_tag_str("subject", subject_name->str),
                evt_tag_str("issuer", issuer_name->str));
    }
  else
    {
      gint errnum, errdepth;

      errnum = X509_STORE_CTX_get_error(ctx);
      errdepth = X509_STORE_CTX_get_error_depth(ctx);
      msg_error("Certificate validation failed",
                evt_tag_str("subject", subject_name->str),
                evt_tag_str("issuer", issuer_name->str),
                evt_tag_str("error", X509_verify_cert_error_string(errnum)),
                evt_tag_int("depth", errdepth));
    }
  g_string_free(subject_name, TRUE);
  g_string_free(issuer_name, TRUE);
}

static gboolean
tls_wildcard_match(const gchar *host_name, const gchar *pattern)
{
  gchar **pattern_parts, **hostname_parts;
  gboolean success = FALSE;
  gchar *lower_pattern = NULL;
  gchar *lower_hostname = NULL;
  gint i;

  pattern_parts = g_strsplit(pattern, ".", 0);
  hostname_parts = g_strsplit(host_name, ".", 0);
  for (i = 0; pattern_parts[i]; i++)
    {
      if (!hostname_parts[i])
        {
          /* number of dot separated entries is not the same in the hostname and the pattern spec */
          goto exit;
        }

      lower_pattern = g_ascii_strdown(pattern_parts[i],-1);
      lower_hostname = g_ascii_strdown(hostname_parts[i],-1);

      if (!g_pattern_match_simple(lower_pattern, lower_hostname))
        goto exit;
    }
  success = TRUE;
exit:
  g_free(lower_pattern);
  g_free(lower_hostname);
  g_strfreev(pattern_parts);
  g_strfreev(hostname_parts);
  return success;
}

gboolean
tls_verify_certificate_name(X509 *cert, const gchar *host_name)
{
  gchar pattern_buf[256];
  gint ext_ndx;
  gboolean found = FALSE, result = FALSE;

  ext_ndx = X509_get_ext_by_NID(cert, NID_subject_alt_name, -1);
  if (ext_ndx >= 0)
    {
      /* ok, there's a subjectAltName extension, check that */
      X509_EXTENSION *ext;
      STACK_OF(GENERAL_NAME) *alt_names;
      GENERAL_NAME *gen_name;

      ext = X509_get_ext(cert, ext_ndx);
      alt_names = X509V3_EXT_d2i(ext);
      if (alt_names)
        {
          gint num, i;

          num = sk_GENERAL_NAME_num(alt_names);

          for (i = 0; !result && i < num; i++)
            {
              gen_name = sk_GENERAL_NAME_value(alt_names, i);
              if (gen_name->type == GEN_DNS)
                {
                  const guchar *dnsname = ASN1_STRING_get0_data(gen_name->d.dNSName);
                  guint dnsname_len = ASN1_STRING_length(gen_name->d.dNSName);

                  if (dnsname_len > sizeof(pattern_buf) - 1)
                    {
                      found = TRUE;
                      result = FALSE;
                      break;
                    }

                  memcpy(pattern_buf, dnsname, dnsname_len);
                  pattern_buf[dnsname_len] = 0;
                  /* we have found a DNS name as alternative subject name */
                  found = TRUE;
                  result = tls_wildcard_match(host_name, pattern_buf);
                }
              else if (gen_name->type == GEN_IPADD)
                {
                  char *dotted_ip = inet_ntoa(*(struct in_addr *) gen_name->d.iPAddress->data);

                  g_strlcpy(pattern_buf, dotted_ip, sizeof(pattern_buf));
                  found = TRUE;
                  result = strcasecmp(host_name, pattern_buf) == 0;
                }
            }
          sk_GENERAL_NAME_free(alt_names);
        }
    }
  if (!found)
    {
      /* hmm. there was no subjectAltName (this is deprecated, but still
       * widely used), look up the Subject, most specific CN */
      X509_NAME *name;

      name = X509_get_subject_name(cert);
      if (X509_NAME_get_text_by_NID(name, NID_commonName, pattern_buf, sizeof(pattern_buf)) != -1)
        {
          result = tls_wildcard_match(host_name, pattern_buf);
        }
    }
  if (!result)
    {
      msg_error("Certificate subject does not match configured hostname",
                evt_tag_str("hostname", host_name),
                evt_tag_str("certificate", pattern_buf));
    }
  else
    {
      msg_verbose("Certificate subject matches configured hostname",
                  evt_tag_str("hostname", host_name),
                  evt_tag_str("certificate", pattern_buf));
    }

  return result;
}

