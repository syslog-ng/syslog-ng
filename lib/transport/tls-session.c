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
#include "transport/tls-session.h"
#include "transport/tls-context.h"
#include "str-utils.h"

#include <glib/gstdio.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/pkcs12.h>
#include <openssl/ocsp.h>

/* TLSSession */

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

static gboolean
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
  if (!ok && tls_context_ignore_validity_period(self->ctx) &&
      (X509_STORE_CTX_get_error(ctx) == X509_V_ERR_CERT_NOT_YET_VALID ||
       X509_STORE_CTX_get_error(ctx) == X509_V_ERR_CERT_HAS_EXPIRED))
    {
      msg_notice("Ignoring not yet valid / expired certificate error due to ssl_options(ignore-validity-period)",
                 tls_context_format_location_tag(self->ctx));
      return 1;
    }
  return ok;
}

static void
_log_certificate_validation_progress(int ok, X509_STORE_CTX *ctx)
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

      _log_certificate_validation_progress(ok, ctx);

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

int
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
  if (!self->peer_info.found && where == (SSL_ST_ACCEPT|SSL_CB_LOOP))
    {
      X509 *cert = SSL_get_peer_certificate(ssl);

      if(cert)
        {
          self->peer_info.found = 1; /* mark this found so we don't keep checking on every callback */
          X509_NAME *name = X509_get_subject_name(cert);

          X509_NAME_get_text_by_NID(name, NID_commonName, self->peer_info.cn, X509_MAX_CN_LEN);
          X509_NAME_get_text_by_NID(name, NID_organizationName, self->peer_info.o, X509_MAX_O_LEN);
          X509_NAME_get_text_by_NID(name, NID_organizationalUnitName, self->peer_info.ou, X509_MAX_OU_LEN);

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

TLSSession *
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
