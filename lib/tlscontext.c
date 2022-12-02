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
#include "secret-storage/secret-storage.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/pkcs12.h>
#include <openssl/ocsp.h>

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
  gchar *location;
};

typedef enum
{
  TLS_CONTEXT_OK,
  TLS_CONTEXT_ERROR,
  TLS_CONTEXT_FILE_ACCES_ERROR,
  TLS_CONTEXT_PASSWORD_ERROR
} TLSContextLoadResult;

void
tls_session_configure_allow_compress(TLSSession *tls_session, gboolean allow_compress)
{
  if (allow_compress)
    {
      SSL_clear_options(tls_session->ssl, SSL_OP_NO_COMPRESSION);
    }
  else
    {
      SSL_set_options(tls_session->ssl, SSL_OP_NO_COMPRESSION);
    }
}

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

static X509 *
tls_session_find_issuer(TLSSession *self, X509 *cert)
{
  STACK_OF(X509) *intermediate_certs = SSL_get_peer_cert_chain(self->ssl);

  for (int i = 0; intermediate_certs && i < sk_X509_num(intermediate_certs); ++i)
    {
      X509 *issuer = sk_X509_value(intermediate_certs, i);
      if (X509_check_issued(issuer, cert) == X509_V_OK)
        {
          return X509_dup(issuer);
        }
    }

  X509_STORE *store = SSL_CTX_get_cert_store(self->ctx->ssl_ctx);
  if (!store)
    return NULL;

  X509_STORE_CTX *store_ctx = X509_STORE_CTX_new();
  if (!store_ctx)
    return NULL;

  if (X509_STORE_CTX_init(store_ctx, store, NULL, NULL) != 1)
    {
      X509_STORE_CTX_free(store_ctx);
      return NULL;
    }

  X509 *issuer = NULL;
  if (X509_STORE_CTX_get1_issuer(&issuer, store_ctx, cert) != 1)
    {
      X509_STORE_CTX_free(store_ctx);
      return NULL;
    }

  X509_STORE_CTX_free(store_ctx);

  return issuer;
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
      msg_notice("Certificate valid, but fingerprint constraints were not met, rejecting",
                 tls_context_format_location_tag(self->ctx));
      return 0;
    }

  X509 *current_cert = X509_STORE_CTX_get_current_cert(ctx);
  if (ok && ctx_error_depth != 0 && (X509_get_extension_flags(current_cert) & EXFLAG_CA) == 0)
    {
      msg_notice("Invalid certificate found in chain, basicConstraints.ca is unset in non-leaf certificate",
                 tls_context_format_location_tag(self->ctx));
      X509_STORE_CTX_set_error(ctx, X509_V_ERR_INVALID_CA);
      return 0;
    }

  /* reject certificate if it is valid, but its DN is not trusted */
  if (ok && ctx_error_depth == 0 && !tls_session_verify_dn(ctx))
    {
      msg_notice("Certificate valid, but DN constraints were not met, rejecting",
                 tls_context_format_location_tag(self->ctx));
      X509_STORE_CTX_set_error(ctx, X509_V_ERR_CERT_UNTRUSTED);
      return 0;
    }
  /* if the crl_dir is set in the configuration file but the directory is empty ignore this error */
  if (!ok && X509_STORE_CTX_get_error(ctx) == X509_V_ERR_UNABLE_TO_GET_CRL)
    {
      msg_notice("CRL directory is set but no CRLs found",
                 tls_context_format_location_tag(self->ctx));
      return 1;
    }

  if (!ok && X509_STORE_CTX_get_error(ctx) == X509_V_ERR_INVALID_PURPOSE)
    {
      msg_warning("Certificate valid, but purpose is invalid",
                  tls_context_format_location_tag(self->ctx));
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
          msg_notice("Error occurred during certificate validation",
                     evt_tag_int("error", X509_STORE_CTX_get_error(ctx)),
                     tls_context_format_location_tag(self->ctx));
          break;
        }
    }
  else
    {
      ok = tls_session_verify(self, ok, ctx);

      tls_log_certificate_validation_progress(ok, ctx);

      if (self->verifier && self->verifier->verify_func)
        return self->verifier->verify_func(ok, ctx, self->verifier->verify_data);
    }
  return ok;
}

static inline gboolean
_ocsp_client_retrieve_response(SSL *ssl, OCSP_RESPONSE **response, OCSP_BASICRESP **basic_response, GError **error)
{
  const unsigned char *ocsp_response_der;
  long ocsp_response_der_length = SSL_get_tlsext_status_ocsp_resp(ssl, &ocsp_response_der);
  if (!ocsp_response_der || ocsp_response_der_length <= 0)
    {
      g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_INTERNAL_ERROR,
                  "no OCSP response was received from the server");
      return FALSE;
    }

  OCSP_RESPONSE *ocsp_response = d2i_OCSP_RESPONSE(NULL, &ocsp_response_der, ocsp_response_der_length);
  if (!ocsp_response)
    {
      g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_INTERNAL_ERROR,
                  "OCSP response received from server can not be parsed");
      return FALSE;
    }

  if (OCSP_response_status(ocsp_response) != OCSP_RESPONSE_STATUS_SUCCESSFUL)
    {
      g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_INTERNAL_ERROR, "OCSP response is unsuccessful");
      OCSP_RESPONSE_free(ocsp_response);
      return FALSE;
    }

  OCSP_BASICRESP *ocsp_basic_resp = OCSP_response_get1_basic(ocsp_response);
  if (!ocsp_basic_resp)
    {
      g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_INTERNAL_ERROR, "can not extract OCSP basic response");
      OCSP_RESPONSE_free(ocsp_response);
      return FALSE;
    }

  *response = ocsp_response;
  *basic_response = ocsp_basic_resp;
  return TRUE;
}

static inline OCSP_CERTID *
_get_ocsp_certid(TLSSession *self, X509 *cert, GError **error)
{
  X509 *issuer = tls_session_find_issuer(self, cert);
  if (!issuer)
    {
      g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_INTERNAL_ERROR, "failed to find certificate issuer");
      return FALSE;
    }

  OCSP_CERTID *cert_id = OCSP_cert_to_id(NULL, cert, issuer);
  if (!cert_id)
    {
      g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_INTERNAL_ERROR, "failed to retrieve certificate ID");
      X509_free(issuer);
      return FALSE;
    }

  X509_free(issuer);

  return cert_id;
}

static gboolean
tls_session_ocsp_client_check_cert_validity(TLSSession *self, OCSP_BASICRESP *ocsp_basic_resp, X509 *cert,
                                            GError **error)
{
  OCSP_CERTID *cert_id = _get_ocsp_certid(self, cert, error);
  if (!cert_id)
    return FALSE;

  int status, reason;
  ASN1_GENERALIZEDTIME *rev_time = NULL;
  ASN1_GENERALIZEDTIME *this_upd = NULL;
  ASN1_GENERALIZEDTIME *next_upd = NULL;
  if (OCSP_resp_find_status(ocsp_basic_resp, cert_id, &status, &reason, &rev_time, &this_upd, &next_upd) != 1)
    {
      g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_INTERNAL_ERROR, "failed to retrieve OCSP response status");
      OCSP_CERTID_free(cert_id);
      return FALSE;
    }

  OCSP_CERTID_free(cert_id);

  switch (status)
    {
    case V_OCSP_CERTSTATUS_GOOD:
      break;
    case V_OCSP_CERTSTATUS_REVOKED:
      g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_INTERNAL_ERROR,
                  "certificate is revoked (reason: %s (%d))", OCSP_crl_reason_str(reason), reason);
      return FALSE;
    default:
      g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_INTERNAL_ERROR, "certificate status is unknown");
      return FALSE;
    }

  if (OCSP_check_validity(this_upd, next_upd, 300L, -1L) != 1)
    {
      g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_INTERNAL_ERROR,
                  "OCSP response is outside its time validity period");
      return FALSE;
    }

  return TRUE;
}

static gboolean
tls_session_ocsp_client_verify(TLSSession *self, OCSP_BASICRESP *ocsp_basic_resp, GError **error)
{
  SSL_CTX *ssl_ctx = SSL_get_SSL_CTX(self->ssl);
  X509_STORE *trusted_cert_store = SSL_CTX_get_cert_store(ssl_ctx);
  STACK_OF(X509) *untrusted_intermediate_certs = SSL_get_peer_cert_chain(self->ssl);

  if (OCSP_basic_verify(ocsp_basic_resp, untrusted_intermediate_certs, trusted_cert_store, 0) != 1)
    {
      g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_INTERNAL_ERROR, "failed to verify OCSP response signature");
      return FALSE;
    }

  X509 *cert = SSL_get_peer_certificate(self->ssl);
  if (!cert)
    {
      g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_INTERNAL_ERROR, "no certificate was presented by server");
      return FALSE;
    }

  if (!tls_session_ocsp_client_check_cert_validity(self, ocsp_basic_resp, cert, error))
    {
      X509_free(cert);
      return FALSE;
    }

  X509_free(cert);

  /* TODO: The server MAY send multiple OCSP responses, one for each cert in the chain,
   * those have to be validated by tls_session_ocsp_client_check_cert_validity(), _if_ they exist.
   */

  return TRUE;
}

static int
tls_session_ocsp_client_verify_callback(SSL *ssl, void *user_data)
{
  TLSSession *self = SSL_get_app_data(ssl);

  OCSP_RESPONSE *ocsp_response = NULL;
  OCSP_BASICRESP *ocsp_basic_resp = NULL;
  GError *error = NULL;

  if (!_ocsp_client_retrieve_response(ssl, &ocsp_response, &ocsp_basic_resp, &error))
    goto err;

  if (!tls_session_ocsp_client_verify(self, ocsp_basic_resp, &error))
    goto err;

  OCSP_BASICRESP_free(ocsp_basic_resp);
  OCSP_RESPONSE_free(ocsp_response);

  msg_debug("OCSP stapling verification succeeded", tls_context_format_location_tag(self->ctx));
  return 1;

err:
  msg_error("OCSP stapling verification failed",
            evt_tag_str("error", error->message),
            tls_context_format_location_tag(self->ctx));

  g_clear_error(&error);
  OCSP_BASICRESP_free(ocsp_basic_resp);
  OCSP_RESPONSE_free(ocsp_response);

  return 0;
}

void
tls_session_set_trusted_fingerprints(TLSContext *self, GList *fingerprints)
{
  g_assert(fingerprints);

  g_list_foreach(self->trusted_fingerprint_list, (GFunc) g_free, NULL);
  self->trusted_fingerprint_list = fingerprints;
}

void
tls_session_set_trusted_dn(TLSContext *self, GList *dn)
{
  g_assert(dn);

  g_list_foreach(self->trusted_dn_list, (GFunc) g_free, NULL);
  self->trusted_dn_list = dn;
}

void
tls_session_set_verifier(TLSSession *self, TLSVerifier *verifier)
{
  self->verifier = verifier ? tls_verifier_ref(verifier) : NULL;
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

static gboolean
_set_sni_in_client_mode(TLSSession *self)
{
  if (!self->ctx->sni)
    return TRUE;

  if (self->ctx->mode != TM_CLIENT)
    return TRUE;

  if (SSL_set_tlsext_host_name(self->ssl, self->ctx->sni))
    return TRUE;

  msg_error("Failed to set SNI",
            evt_tag_str("sni", self->ctx->sni),
            tls_context_format_location_tag(self->ctx));

  return FALSE;
}

void
_write_line_to_keylog_file(const char *file_path, const char *line, FILE *keylog_file, GMutex *mutex)
{
  if(!keylog_file)
    return;


  g_mutex_lock(mutex);
  gint ret_val = fprintf(keylog_file, "%s\n", line);
  if (ret_val != strlen(line)+1)
    msg_error("Couldn't write to TLS keylogfile", evt_tag_errno("error", ret_val));

  fflush(keylog_file);
  g_mutex_unlock(mutex);
}

static inline void
_dump_tls_keylog(const SSL *ssl, const char *line)
{
  if(!ssl)
    return;

  TLSSession *self = SSL_get_app_data(ssl);
  _write_line_to_keylog_file(self->ctx->keylog_file_path, line, self->ctx->keylog_file, &self->ctx->keylog_file_lock);
}

static gboolean
tls_session_keylog_setup_file(TLSSession *self, const char *keylog_file_path)
{
  self->ctx->keylog_file = g_fopen(keylog_file_path, "a");
  if(!self->ctx->keylog_file)
    {
      msg_error("Error opening keylog-file",
                evt_tag_str(EVT_TAG_FILENAME, keylog_file_path),
                evt_tag_error(EVT_TAG_OSERROR));
      return FALSE;
    }
  return TRUE;
}

static TLSSession *
tls_session_new(SSL *ssl, TLSContext *ctx)
{
  TLSSession *self = g_new0(TLSSession, 1);

  self->ssl = ssl;
  self->ctx = tls_context_ref(ctx);

  /* to set verify callback */
  tls_session_set_verifier(self, NULL);

  SSL_set_info_callback(ssl, tls_session_info_callback);

  if (!_set_sni_in_client_mode(self))
    {
      tls_context_unref(self->ctx);
      g_free(self);
      return NULL;
    }
  if(ctx->keylog_file_path)
    {
      if(!tls_session_keylog_setup_file(self, self->ctx->keylog_file_path))
        {
          tls_context_unref(self->ctx);
          g_free(self);
          return NULL;
        }
    }

  return self;
}

void
tls_session_free(TLSSession *self)
{
  tls_context_unref(self->ctx);
  if (self->verifier)
    tls_verifier_unref(self->verifier);
  SSL_free(self->ssl);

  g_free(self);
}

EVTTAG *
tls_context_format_tls_error_tag(TLSContext *self)
{
  gint ssl_error = ERR_get_error();

  char buf[256];
  ERR_error_string_n(ssl_error, buf, sizeof(buf));

  return evt_tag_str("tls_error", buf);
}

EVTTAG *
tls_context_format_location_tag(TLSContext *self)
{
  return evt_tag_str("location", self->location);
}

static gboolean
_is_file_accessible(TLSContext *self, const gchar *fname)
{
  if (!fname)
    return FALSE;
  if (access(fname, R_OK) < 0)
    {
      msg_error("Error opening TLS related key or certificate file",
                evt_tag_str("filename", fname),
                evt_tag_error("error"),
                tls_context_format_location_tag(self));

      return FALSE;
    }
  return TRUE;
}

static void
_print_and_clear_tls_session_error(TLSContext *self)
{
  msg_error("Error setting up TLS session context",
            tls_context_format_tls_error_tag(self),
            tls_context_format_location_tag(self));
  ERR_clear_error();
}

static void
tls_context_setup_session_tickets(TLSContext *self)
{
  openssl_ctx_setup_session_tickets(self->ssl_ctx);
}

static void
tls_context_setup_verify_mode(TLSContext *self)
{
  gint verify_mode = 0;

  switch (self->verify_mode)
    {
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
tls_context_setup_ocsp_stapling(TLSContext *self)
{
  if (self->mode == TM_CLIENT && self->ocsp_stapling_verify)
    {
      long status_cb_set = SSL_CTX_set_tlsext_status_cb(self->ssl_ctx, tls_session_ocsp_client_verify_callback);
      g_assert(status_cb_set);
      return;
    }

  if (self->mode == TM_SERVER)
    {
      g_assert(!self->ocsp_stapling_verify
               && "OCSP stapling and its verification are currently not implemented on the server side");
    }
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
#ifdef SSL_OP_NO_TLSv1_3
      if(self->ssl_options & TSO_NOTLSv13)
        ssl_options |= SSL_OP_NO_TLSv1_3;
#endif
#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE
      if (self->mode == TM_SERVER)
        ssl_options |= SSL_OP_CIPHER_SERVER_PREFERENCE;
#endif
      SSL_CTX_set_options(self->ssl_ctx, ssl_options);
    }
  else
    {
      msg_debug("No special SSL options were specified",
                tls_context_format_location_tag(self));
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
tls_context_setup_ecdh(TLSContext *self)
{
  if (!_set_optional_ecdh_curve_list(self->ssl_ctx, self->ecdh_curve_list))
    return FALSE;

  openssl_ctx_setup_ecdh(self->ssl_ctx);

  return TRUE;
}

static gboolean
tls_context_setup_dh(TLSContext *self)
{
  if (!self->dhparam_file)
    return openssl_ctx_setup_dh(self->ssl_ctx);

  if(!_is_file_accessible(self, self->dhparam_file))
    return FALSE;

  if (!openssl_ctx_load_dh_from_file(self->ssl_ctx, self->dhparam_file))
    {
      msg_error("Error setting up TLS session context, invalid DH parameters",
                evt_tag_str("dhparam_file", self->dhparam_file));
      return FALSE;
    }

  return TRUE;
}

static gboolean
tls_context_setup_cipher_suite(TLSContext *self)
{
  if (self->cipher_suite && !SSL_CTX_set_cipher_list(self->ssl_ctx, self->cipher_suite))
    return FALSE;

#if SYSLOG_NG_HAVE_DECL_SSL_CTX_SET_CIPHERSUITES
  if (self->tls13_cipher_suite && !SSL_CTX_set_ciphersuites(self->ssl_ctx, self->tls13_cipher_suite))
    return FALSE;
#endif

  return TRUE;
}

static gboolean
tls_context_setup_sigalgs(TLSContext *self)
{
#if SYSLOG_NG_HAVE_DECL_SSL_CTX_SET1_SIGALGS_LIST
  if (self->sigalgs && !SSL_CTX_set1_sigalgs_list(self->ssl_ctx, self->sigalgs))
    return FALSE;
#endif

#if SYSLOG_NG_HAVE_DECL_SSL_CTX_SET1_CLIENT_SIGALGS_LIST
  if (self->client_sigalgs && !SSL_CTX_set1_client_sigalgs_list(self->ssl_ctx, self->client_sigalgs))
    return FALSE;
#endif

  return TRUE;
}

static gboolean
tls_context_setup_cmd_context(TLSContext *self)
{
  SSL_CONF_CTX *ssl_conf_ctx = SSL_CONF_CTX_new();
  const int ctx_flags = SSL_CONF_FLAG_FILE | SSL_CONF_FLAG_CLIENT | SSL_CONF_FLAG_SERVER |
                        SSL_CONF_FLAG_CERTIFICATE | SSL_CONF_FLAG_SHOW_ERRORS;
  int set_flags = SSL_CONF_CTX_set_flags(ssl_conf_ctx, ctx_flags);

  gboolean result = (set_flags == ctx_flags);
  if (result)
    {
      SSL_CONF_CTX_set_ssl_ctx(ssl_conf_ctx, self->ssl_ctx);

      for (GList *next_pair = self->conf_cmds_list; next_pair; next_pair = next_pair->next)
        {
          gchar *cmd = next_pair->data;
          next_pair = next_pair->next;
          gchar *value = next_pair->data;

          int add_result = SSL_CONF_cmd(ssl_conf_ctx, cmd, value);
          if (add_result < 1)
            msg_error("Error setting up SSL conf context, invalid cmd or value in config list",
                      evt_tag_str("cmd", cmd),
                      evt_tag_str("value", value),
                      tls_context_format_location_tag(self));
          result = result && (add_result >= 1);
        }
      result = result && (SSL_CONF_CTX_finish(ssl_conf_ctx) == 1);
    }

  SSL_CONF_CTX_free(ssl_conf_ctx);
  return result;
}

static PKCS12 *
_load_pkcs12_file(TLSContext *self, const gchar *pkcs12_file)
{
  if (!_is_file_accessible(self, pkcs12_file))
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
  PKCS12 *pkcs12 = _load_pkcs12_file(self, self->pkcs12_file);

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

  gboolean loaded = FALSE;
  if (ca_list && !tls_context_add_certs(self, ca_list))
    goto error;

  loaded = SSL_CTX_use_certificate(self->ssl_ctx, cert)
           && SSL_CTX_use_PrivateKey(self->ssl_ctx, private_key)
           && SSL_CTX_check_private_key(self->ssl_ctx);

error:
  X509_free(cert);
  EVP_PKEY_free(private_key);
  sk_X509_pop_free(ca_list, X509_free);
  return loaded;
}

static gboolean
_are_key_and_cert_files_accessible(TLSContext *self)
{
  gboolean key_file_accessible = _is_file_accessible(self, self->key_file);
  gboolean cert_file_accessible = _is_file_accessible(self, self->cert_file);

  return key_file_accessible && cert_file_accessible;
}

static gboolean
_key_or_cert_file_is_not_specified(TLSContext *self)
{
  return (!self->key_file || !self->cert_file);
}

static TLSContextLoadResult
tls_context_load_key_and_cert(TLSContext *self)
{
  if (_key_or_cert_file_is_not_specified(self))
    {
      if (self->mode == TM_SERVER)
        msg_warning("You have a TLS enabled source without a X.509 keypair. Make sure you have tls(key-file() and cert-file()) options, TLS handshake to this source will fail",
                    tls_context_format_location_tag(self));
      return TLS_CONTEXT_OK;
    }

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
        msg_warning("WARNING: pkcs12-file() is specified, key-file() and cert-file() will be omitted",
                    tls_context_format_location_tag(self));

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

  if (_is_file_accessible(self, self->ca_dir) && !SSL_CTX_load_verify_locations(self->ssl_ctx, NULL, self->ca_dir))
    goto error;

  if (_is_file_accessible(self, self->ca_file) && !SSL_CTX_load_verify_locations(self->ssl_ctx, self->ca_file, NULL))
    goto error;

  if (_is_file_accessible(self, self->crl_dir) && !SSL_CTX_load_verify_locations(self->ssl_ctx, NULL, self->crl_dir))
    goto error;

  if (self->crl_dir)
    verify_flags |= X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL;

  X509_VERIFY_PARAM_set_flags(SSL_CTX_get0_param(self->ssl_ctx), verify_flags);

  if (self->mode == TM_SERVER)
    tls_context_setup_session_tickets(self);

  tls_context_setup_verify_mode(self);
  tls_context_setup_ocsp_stapling(self);

  tls_context_setup_ssl_options(self);
  if (!tls_context_setup_ecdh(self))
    goto error_no_print;

  if (!tls_context_setup_dh(self))
    goto error;

  if (!tls_context_setup_cipher_suite(self))
    goto error;

  if (!tls_context_setup_sigalgs(self))
    goto error;

  if (!tls_context_setup_cmd_context(self))
    goto error;

  return TLS_CONTEXT_SETUP_OK;

error:
  _print_and_clear_tls_session_error(self);
error_no_print:
  if (self->ssl_ctx)
    {
      SSL_CTX_free(self->ssl_ctx);
      self->ssl_ctx = NULL;
    }
  return TLS_CONTEXT_SETUP_ERROR;
password_error:
  _print_and_clear_tls_session_error(self);
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

  if (self->mode == TM_CLIENT && self->ocsp_stapling_verify)
    {
      long ocsp_enabled = SSL_set_tlsext_status_type(ssl, TLSEXT_STATUSTYPE_ocsp);
      g_assert(ocsp_enabled);
    }

  TLSSession *session = tls_session_new(ssl, self);
  if (!session)
    {
      SSL_free(ssl);
      return NULL;
    }

  SSL_set_app_data(ssl, session);
  return session;
}

/* NOTE: location is a string description where this tls context was defined, e.g. the location in the config */
TLSContext *
tls_context_new(TLSMode mode, const gchar *location)
{
  TLSContext *self = g_new0(TLSContext, 1);

  g_atomic_counter_set(&self->ref_cnt, 1);
  self->mode = mode;
  self->verify_mode = TVM_REQUIRED | TVM_TRUSTED;
  self->ssl_options = TSO_NOSSLv2;
  self->location = g_strdup(location ? : "n/a");

  if (self->mode == TM_CLIENT)
    self->ssl_ctx = SSL_CTX_new(SSLv23_client_method());
  else
    {
      self->ssl_ctx = SSL_CTX_new(SSLv23_server_method());
      SSL_CTX_set_session_id_context(self->ssl_ctx, (const unsigned char *) "syslog", 6);
    }

  return self;
}

static void
_tls_context_free(TLSContext *self)
{
  g_free(self->location);
  SSL_CTX_free(self->ssl_ctx);
  g_list_foreach(self->conf_cmds_list, (GFunc) g_free, NULL);
  g_list_foreach(self->trusted_fingerprint_list, (GFunc) g_free, NULL);
  g_list_foreach(self->trusted_dn_list, (GFunc) g_free, NULL);
  g_free(self->key_file);
  g_free(self->pkcs12_file);
  g_free(self->cert_file);
  g_free(self->dhparam_file);
  g_free(self->ca_dir);
  g_free(self->crl_dir);
  g_free(self->ca_file);
  g_free(self->cipher_suite);
  g_free(self->tls13_cipher_suite);
  g_free(self->sigalgs);
  g_free(self->client_sigalgs);
  g_free(self->ecdh_curve_list);
  g_free(self->sni);
  g_free(self->keylog_file_path);

  if(self->keylog_file)
    fclose(self->keylog_file);

  g_free(self);
}

TLSContext *
tls_context_ref(TLSContext *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt) > 0);

  if (self)
    g_atomic_counter_inc(&self->ref_cnt);

  return self;
}

void
tls_context_unref(TLSContext *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt));
  if (self && (g_atomic_counter_dec_and_test(&self->ref_cnt)))
    _tls_context_free(self);
}

TLSVerifier *
tls_verifier_new(TLSSessionVerifyFunc verify_func, gpointer verify_data,
                 GDestroyNotify verify_data_destroy)
{
  TLSVerifier *self = g_new0(TLSVerifier, 1);

  g_atomic_counter_set(&self->ref_cnt, 1);
  self->verify_func = verify_func;
  self->verify_data = verify_data;
  self->verify_data_destroy = verify_data_destroy;
  return self;
}

TLSVerifier *
tls_verifier_ref(TLSVerifier *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt) > 0);

  if (self)
    g_atomic_counter_inc(&self->ref_cnt);

  return self;
}

static void
_tls_verifier_free(TLSVerifier *self)
{
  g_assert(self);

  if (self)
    {
      if (self->verify_data && self->verify_data_destroy)
        self->verify_data_destroy(self->verify_data);
      g_free(self);
    }
}

void
tls_verifier_unref(TLSVerifier *self)
{
  g_assert(!self || g_atomic_counter_get(&self->ref_cnt));

  if (self && (g_atomic_counter_dec_and_test(&self->ref_cnt)))
    _tls_verifier_free(self);
}

gboolean
tls_context_set_verify_mode_by_name(TLSContext *self, const gchar *mode_str)
{
  if (strcasecmp(mode_str, "optional-trusted") == 0 || strcasecmp(mode_str, "optional_trusted") == 0)
    self->verify_mode = TVM_OPTIONAL | TVM_TRUSTED;
  else if (strcasecmp(mode_str, "optional-untrusted") == 0 || strcasecmp(mode_str, "optional_untrusted") == 0
           || strcasecmp(mode_str, "none") == 0)
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
#ifdef SSL_OP_NO_TLSv1_3
      else if (strcasecmp(l->data, "no-tlsv13") == 0 || strcasecmp(l->data, "no_tlsv13") == 0)
        self->ssl_options |= TSO_NOTLSv13;
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

static int
_pem_passwd_callback(char *buf, int size, int rwflag, void *user_data)
{
  if (!user_data)
    return 0;

  gchar *key = (gchar *)user_data;
  Secret *secret = secret_storage_get_secret_by_name(key);
  if (!secret)
    return 0;

  size_t len = secret->len;
  if (secret->len > size)
    {
      len = size;
      msg_warning("Password is too long, will be truncated",
                  evt_tag_int("original_length", secret->len),
                  evt_tag_int("truncated_length", size));
    }
  memcpy(buf, secret->data, len);
  secret_storage_put_secret(secret);

  return len;
}

void
tls_context_set_key_file(TLSContext *self, const gchar *key_file)
{
  g_free(self->key_file);
  self->key_file = g_strdup(key_file);
  SSL_CTX_set_default_passwd_cb(self->ssl_ctx, _pem_passwd_callback);
  SSL_CTX_set_default_passwd_cb_userdata(self->ssl_ctx, self->key_file);
}

gboolean
tls_context_set_keylog_file(TLSContext *self, gchar *keylog_file_path, GError **error)
{
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
  g_free(self->keylog_file_path);
  msg_warning_once("WARNING: TLS keylog file has been set up, it should only be used during debugging sessions");
  self->keylog_file_path = g_strdup(keylog_file_path);
  SSL_CTX_set_keylog_callback(self->ssl_ctx, _dump_tls_keylog);
  return TRUE;
#else
  g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_UNSUPPORTED, "keylog-file() requires OpenSSL >= v1.1.1");
  return FALSE;
#endif
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
tls_context_set_ca_file(TLSContext *self, const gchar *ca_file)
{
  g_free(self->ca_file);
  self->ca_file = g_strdup(ca_file);
}

void
tls_context_set_cipher_suite(TLSContext *self, const gchar *cipher_suite)
{
  g_free(self->cipher_suite);
  self->cipher_suite = g_strdup(cipher_suite);
}

gboolean
tls_context_set_tls13_cipher_suite(TLSContext *self, const gchar *tls13_cipher_suite, GError **error)
{
#if SYSLOG_NG_HAVE_DECL_SSL_CTX_SET_CIPHERSUITES
  g_free(self->tls13_cipher_suite);
  self->tls13_cipher_suite = g_strdup(tls13_cipher_suite);
  return TRUE;
#else
  g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_UNSUPPORTED,
              "Setting TLS 1.3 ciphers is not supported with the OpenSSL version syslog-ng was compiled with");
  return FALSE;
#endif
}

gboolean
tls_context_set_sigalgs(TLSContext *self, const gchar *sigalgs, GError **error)
{
#if SYSLOG_NG_HAVE_DECL_SSL_CTX_SET1_SIGALGS_LIST
  g_free(self->sigalgs);
  self->sigalgs = g_strdup(sigalgs);
  return TRUE;
#else
  g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_UNSUPPORTED,
              "Setting sigalgs is not supported with the OpenSSL version syslog-ng was compiled with");
  return FALSE;
#endif
}

gboolean
tls_context_set_client_sigalgs(TLSContext *self, const gchar *sigalgs, GError **error)
{
#if SYSLOG_NG_HAVE_DECL_SSL_CTX_SET1_CLIENT_SIGALGS_LIST
  g_free(self->client_sigalgs);
  self->client_sigalgs = g_strdup(sigalgs);
  return TRUE;
#else
  g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_UNSUPPORTED,
              "Setting client sigalgs is not supported with the OpenSSL version syslog-ng was compiled with");
  return FALSE;
#endif
}

gboolean
tls_context_set_conf_cmds(TLSContext *self, GList *cmds, GError **error)
{
  g_list_foreach(self->conf_cmds_list, (GFunc) g_free, NULL);
  self->conf_cmds_list = cmds;
  return TRUE;
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
tls_context_set_sni(TLSContext *self, const gchar *sni)
{
  g_free(self->sni);
  self->sni = g_strdup(sni);
}

void
tls_context_set_ocsp_stapling_verify(TLSContext *self, gboolean ocsp_stapling_verify)
{
  self->ocsp_stapling_verify = ocsp_stapling_verify;
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

      lower_pattern = g_ascii_strdown(pattern_parts[i], -1);
      lower_hostname = g_ascii_strdown(hostname_parts[i], -1);

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
                  gchar dotted_ip[64] = {0};
                  int addr_family = AF_INET;
                  if (gen_name->d.iPAddress->length == 16)
                    addr_family = AF_INET6;

                  if (inet_ntop(addr_family, gen_name->d.iPAddress->data, dotted_ip, sizeof(dotted_ip)))
                    {
                      g_strlcpy(pattern_buf, dotted_ip, sizeof(pattern_buf));
                      found = TRUE;
                      result = strcasecmp(host_name, pattern_buf) == 0;
                    }
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

const gchar *
tls_context_get_key_file(TLSContext *self)
{
  return self->key_file;
}

GQuark
tls_context_error_quark(void)
{
  return g_quark_from_static_string("tls-context-error-quark");
}
