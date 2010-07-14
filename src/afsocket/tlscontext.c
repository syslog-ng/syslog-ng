/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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

#include "tlscontext.h"
#include "misc.h"
#include "messages.h"
#include "apphook.h"

#if ENABLE_SSL

#include <arpa/inet.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>

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
  SSL *ssl = X509_STORE_CTX_get_app_data(ctx);
  TLSSession *self = SSL_get_app_data(ssl);
  GList *current_fingerprint = self->ctx->trusted_fingerpint_list;
  GString *hash;
  gboolean match = FALSE;
  X509 *cert = X509_STORE_CTX_get_current_cert(ctx);

  if (!current_fingerprint || !cert)
    return match;

  hash = g_string_sized_new(EVP_MAX_MD_SIZE * 3);

  if (tls_get_x509_digest(cert, hash))
    {
      do
        {
          if (strcmp((const gchar*)(current_fingerprint->data), hash->str) == 0)
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
  X509_NAME_print_ex(bio, name, 0, ASN1_STRFLGS_ESC_2253 | ASN1_STRFLGS_UTF8_CONVERT | XN_FLAG_SEP_CPLUS_SPC | XN_FLAG_DN_REV);
  len = BIO_get_mem_data(bio, &buf);
  g_string_assign_len(dn, buf, len);
  BIO_free(bio);
}

int
tls_session_verify_dn(X509_STORE_CTX *ctx)
{
  SSL *ssl = X509_STORE_CTX_get_app_data(ctx);
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

  /* accept certificate if its fingerprint matches, again regardless whether x509 certificate validation was successful */
  if (tls_session_verify_fingerprint(ctx))
    {
      msg_debug("Certificate accepted because its fingerprint is listed", NULL);
      return 1;
    }

  if (ok && ctx->error_depth != 0 && (ctx->current_cert->ex_flags & EXFLAG_CA) == 0)
    {
      msg_debug("Invalid certificate found in chain, basicConstraints.ca is unset in non-leaf certificate", NULL);
      ctx->error = X509_V_ERR_INVALID_CA;
      return 0;
    }

  /* reject certificate if it is valid, but its DN is not trusted */
  if (ok && ctx->error_depth == 0 && !tls_session_verify_dn(ctx))
    {
      msg_debug("Certificate valid, but DN constraints were not met, rejecting", NULL);
      ctx->error = X509_V_ERR_CERT_UNTRUSTED;
      return 0;
    }
  /* if the crl_dir is set in the configuration file but the directory is empty ignore this error */
  if (!ok && ctx->error == X509_V_ERR_UNABLE_TO_GET_CRL)
    {
      msg_notice("CRL directory is set but no CRLs found", NULL);
      return 1;
    }

  return ok;
}

int
tls_session_verify_callback(int ok, X509_STORE_CTX *ctx)
{
  SSL *ssl = X509_STORE_CTX_get_app_data(ctx);
  TLSSession *self = SSL_get_app_data(ssl);

  ok = tls_session_verify(self, ok, ctx);

  tls_log_certificate_validation_progress(ok, ctx);

  if (self->verify_func)
    return self->verify_func(ok, ctx, self->verify_data);
  return ok;
}

void
tls_session_set_trusted_fingerprints(TLSContext *self, GList *fingerprints)
{
  g_assert(fingerprints);

  self->trusted_fingerpint_list = fingerprints;
}

void
tls_session_set_trusted_dn(TLSContext *self, GList *dn)
{
  g_assert(dn);

  self->trusted_dn_list = dn;
}

void
tls_session_set_verify(TLSSession *self, TLSSessionVerifyFunc verify_func, gpointer verify_data, GDestroyNotify verify_destroy)
{
  self->verify_func = verify_func;
  self->verify_data = verify_data;
  self->verify_data_destroy = verify_destroy;
}

static TLSSession *
tls_session_new(SSL *ssl, TLSContext *ctx)
{
  TLSSession *self = g_new0(TLSSession, 1);

  self->ssl = ssl;
  self->ctx = ctx;

  /* to set verify callback */
  tls_session_set_verify(self, NULL, NULL, NULL);
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
                evt_tag_errno("error", errno),
                NULL);
      return FALSE;
    }
  return TRUE;
}

TLSSession *
tls_context_setup_session(TLSContext *self)
{
  SSL *ssl;
  TLSSession *session;
  gint ssl_error;

  if (!self->ssl_ctx)
    {
      gint verify_mode = 0;
      gint verify_flags = X509_V_FLAG_POLICY_CHECK;

      if (self->mode == TM_CLIENT)
        self->ssl_ctx = SSL_CTX_new(SSLv23_client_method());
      else
        self->ssl_ctx = SSL_CTX_new(SSLv23_server_method());

      if (!self->ssl_ctx)
        goto error;
      if (file_exists(self->key_file) && !SSL_CTX_use_PrivateKey_file(self->ssl_ctx, self->key_file, SSL_FILETYPE_PEM))
        goto error;

      if (file_exists(self->cert_file) && !SSL_CTX_use_certificate_file(self->ssl_ctx, self->cert_file, SSL_FILETYPE_PEM))
        goto error;
      if (self->key_file && self->cert_file && !SSL_CTX_check_private_key(self->ssl_ctx))
        goto error;

      if (file_exists(self->ca_dir) && !SSL_CTX_load_verify_locations(self->ssl_ctx, NULL, self->ca_dir))
        goto error;

      if (file_exists(self->crl_dir) && !SSL_CTX_load_verify_locations(self->ssl_ctx, NULL, self->crl_dir))
        goto error;

      if (self->crl_dir)
        verify_flags |= X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL;

      X509_VERIFY_PARAM_set_flags(self->ssl_ctx->param, verify_flags);

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
      SSL_CTX_set_options(self->ssl_ctx, SSL_OP_NO_SSLv2);
    }

  ssl = SSL_new(self->ssl_ctx);

  if (self->mode == TM_CLIENT)
    SSL_set_connect_state(ssl);
  else
    SSL_set_accept_state(ssl);

  session = tls_session_new(ssl, self);
  SSL_set_app_data(ssl, session);
  return session;

 error:
  ssl_error = ERR_get_error();
  msg_error("Error setting up TLS session context",
            evt_tag_printf("tls_error", "%s:%s:%s", ERR_lib_error_string(ssl_error), ERR_func_error_string(ssl_error), ERR_reason_error_string(ssl_error)),
            NULL);
  ERR_clear_error();
  if (self->ssl_ctx)
    {
      SSL_CTX_free(self->ssl_ctx);
      self->ssl_ctx = NULL;
    }
  return NULL;
}

TLSContext *
tls_context_new(TLSMode mode)
{
  TLSContext *self = g_new0(TLSContext, 1);

  self->mode = mode;
  self->verify_mode = TVM_REQUIRED | TVM_TRUSTED;
  return self;
}

void
tls_context_free(TLSContext *self)
{
  SSL_CTX_free(self->ssl_ctx);
  g_list_foreach(self->trusted_fingerpint_list, (GFunc) g_free, NULL);
  g_list_foreach(self->trusted_dn_list, (GFunc) g_free, NULL);
  g_free(self->key_file);
  g_free(self->cert_file);
  g_free(self->ca_dir);
  g_free(self->crl_dir);
  g_free(self);
}

TLSVerifyMode
tls_lookup_verify_mode(const gchar *mode_str)
{
  if (strcasecmp(mode_str, "none") == 0)
    return TVM_NONE;
  else if (strcasecmp(mode_str, "optional-trusted") == 0 || strcasecmp(mode_str, "optional_trusted") == 0)
    return TVM_OPTIONAL | TVM_TRUSTED;
  else if (strcasecmp(mode_str, "optional-untrusted") == 0 || strcasecmp(mode_str, "optional_untrusted") == 0)
    return TVM_OPTIONAL | TVM_UNTRUSTED;
  else if (strcasecmp(mode_str, "required-trusted") == 0 || strcasecmp(mode_str, "required_trusted") == 0)
    return TVM_REQUIRED | TVM_TRUSTED;
  else if (strcasecmp(mode_str, "required-untrusted") == 0 || strcasecmp(mode_str, "required_untrusted") == 0)
    return TVM_REQUIRED | TVM_UNTRUSTED;

  return TVM_REQUIRED | TVM_TRUSTED;
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
      msg_verbose("Certificate validation progress",
                  evt_tag_str("subject", subject_name->str),
                  evt_tag_str("issuer", issuer_name->str),
                  NULL);
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
                evt_tag_int("depth", errdepth),
                NULL);
    }
  g_string_free(subject_name, TRUE);
  g_string_free(issuer_name, TRUE);
}

static gboolean
tls_wildcard_match(const gchar *host_name, const gchar *pattern)
{
  gchar **pattern_parts, **hostname_parts;
  gboolean success = FALSE;
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
      if (!g_pattern_match_simple(pattern_parts[i], hostname_parts[i]))
        goto exit;
    }
  success = TRUE;
 exit:
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
                  guchar *dnsname = ASN1_STRING_data(gen_name->d.dNSName);
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
                  result = strcmp(host_name, pattern_buf) == 0;
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
                evt_tag_str("certificate", pattern_buf),
                NULL);
    }
  else
    {
      msg_notice("Certificate subject matches configured hostname",
                evt_tag_str("hostname", host_name),
                evt_tag_str("certificate", pattern_buf),
                NULL);
    }

  return result;
}

static void
tls_deinit(void)
{
  char rnd_file[256];

  if (seed_rng)
    {
      RAND_file_name(rnd_file, sizeof(rnd_file));
      if (rnd_file[0])
        RAND_write_file(rnd_file);
    }
}

void
tls_init(void)
{
  char rnd_file[256];

  if (seed_rng)
    {
      RAND_file_name(rnd_file, sizeof(rnd_file));
      if (rnd_file[0])
        RAND_load_file(rnd_file, -1);
    }
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();
  register_application_hook(AH_SHUTDOWN, (ApplicationHookFunc) tls_deinit, NULL);
}


#endif
