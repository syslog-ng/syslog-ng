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

#include "transport/tls-context.h"
#include "messages.h"
#include "compat/openssl_support.h"
#include "secret-storage/secret-storage.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/pkcs12.h>
#include <openssl/ocsp.h>

typedef enum
{
  TLS_CONTEXT_OK,
  TLS_CONTEXT_ERROR,
  TLS_CONTEXT_FILE_ACCES_ERROR,
  TLS_CONTEXT_PASSWORD_ERROR
} TLSContextLoadResult;

/* TLSContext */

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

#if OPENSSL_VERSION_NUMBER >= 0x10101000L

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

static void
_dump_tls_keylog(const SSL *ssl, const char *line)
{
  if(!ssl)
    return;

  SSL_CTX *ssl_ctx = SSL_get_SSL_CTX(ssl);
  TLSContext *self = SSL_CTX_get_app_data(ssl_ctx);
  _write_line_to_keylog_file(self->keylog_file_path, line, self->keylog_file, &self->keylog_file_lock);
}

static gboolean
_setup_keylog_file(TLSContext *self)
{
  if (!self->keylog_file_path)
    return TRUE;

  self->keylog_file = fopen(self->keylog_file_path, "a");
  if(!self->keylog_file)
    {
      msg_error("Error opening keylog-file",
                evt_tag_str(EVT_TAG_FILENAME, self->keylog_file_path),
                evt_tag_error(EVT_TAG_OSERROR));
      return FALSE;
    }
  SSL_CTX_set_keylog_callback(self->ssl_ctx, _dump_tls_keylog);

  return TRUE;
}

#else

static gboolean
_setup_keylog_file(TLSContext *self)
{
  return TRUE;
}

#endif

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
tls_context_setup_ssl_version(TLSContext *self)
{
  if (self->ssl_version == 0)
    return;
#if SYSLOG_NG_HAVE_DECL_SSL_CTX_SET_MIN_PROTO_VERSION
  SSL_CTX_set_min_proto_version(self->ssl_ctx, self->ssl_version);
#endif
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

#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
      if (self->ssl_options & TSO_IGNORE_UNEXPECTED_EOF)
        ssl_options |= SSL_OP_IGNORE_UNEXPECTED_EOF;
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
#if SYSLOG_NG_HAVE_DECL_SSL_CONF_CTX_NEW
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
#else
  if (self->conf_cmds_list != NULL)
    return FALSE;
  else
    return TRUE;
#endif
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

static void
_ca_list_free(STACK_OF(X509_NAME) *ca_list)
{
  for (gint i = 0; i < sk_X509_NAME_num(ca_list); i++)
    X509_NAME_free(sk_X509_NAME_value(ca_list, i));
  sk_X509_NAME_free(ca_list);
}

static gboolean
_set_client_ca_list(TLSContext *self)
{
#if SYSLOG_NG_HAVE_DECL_SSL_ADD_DIR_CERT_SUBJECTS_TO_STACK && SYSLOG_NG_HAVE_DECL_SSL_ADD_FILE_CERT_SUBJECTS_TO_STACK

  STACK_OF(X509_NAME) *ca_list = sk_X509_NAME_new_null();
  const STACK_OF(X509_NAME) *existing_ca_list = SSL_CTX_get_client_CA_list(self->ssl_ctx);

  for (gint i = 0; i < sk_X509_NAME_num(existing_ca_list); i++)
    {
      sk_X509_NAME_push(ca_list, X509_NAME_dup(sk_X509_NAME_value(existing_ca_list, i)));
    }

  if (_is_file_accessible(self, self->ca_dir) && !SSL_add_dir_cert_subjects_to_stack(ca_list, self->ca_dir))
    {
      _ca_list_free(ca_list);
      return FALSE;
    }

  if (_is_file_accessible(self, self->ca_file) && !SSL_add_file_cert_subjects_to_stack(ca_list, self->ca_file))
    {
      _ca_list_free(ca_list);
      return FALSE;
    }

  SSL_CTX_set_client_CA_list(self->ssl_ctx, ca_list);
  return TRUE;
#else
  msg_warning_once("Setting the client CA list based on the ca-file() and ca-dir() options is not supported with the "
                   "OpenSSL version syslog-ng was compiled with");
  return TRUE;
#endif
}

gboolean
tls_context_verify_peer(TLSContext *self, X509 *peer_cert, const gchar *peer_name)
{
  if ((tls_context_get_verify_mode(self) & TVM_TRUSTED) == 0)
    {
      msg_warning("Bypassing certificate validation, peer certificate is always accepted");
      return TRUE;
    }

  if (!peer_name)
    return TRUE;

  if (!tls_verify_certificate_name(peer_cert, peer_name))
    {
      if (tls_context_ignore_hostname_mismatch(self))
        {
          msg_warning("Ignoring certificate subject validation error due to options(ignore-hostname-mismatch)",
                      evt_tag_str("hostname", peer_name));
          return TRUE;
        }
      return FALSE;
    }

  return TRUE;
}

TLSContextSetupResult
tls_context_setup_context(TLSContext *self)
{
  gint verify_flags = X509_V_FLAG_POLICY_CHECK;

  if (!self->ssl_ctx)
    goto error;

  if (!_setup_keylog_file(self))
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

  if (self->mode == TM_SERVER && !_set_client_ca_list(self))
    goto error;

  if (self->crl_dir)
    verify_flags |= X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL;

  X509_VERIFY_PARAM_set_flags(SSL_CTX_get0_param(self->ssl_ctx), verify_flags);

  if (self->mode == TM_SERVER)
    tls_context_setup_session_tickets(self);

  tls_context_setup_verify_mode(self);
  tls_context_setup_ocsp_stapling(self);

  tls_context_setup_ssl_version(self);
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
tls_context_set_ssl_version_by_name(TLSContext *self, const gchar *value)
{
#if SYSLOG_NG_HAVE_DECL_SSL_CTX_SET_MIN_PROTO_VERSION
  if (strcasecmp(value, "sslv3") == 0)
    self->ssl_version = SSL3_VERSION;
  else if (strcasecmp(value, "tlsv1") == 0 || strcasecmp(value, "tlsv1_0") == 0)
    self->ssl_version = TLS1_VERSION;
  else if (strcasecmp(value, "tlsv1_1") == 0)
    self->ssl_version = TLS1_1_VERSION;
  else if (strcasecmp(value, "tlsv1_2") == 0)
    self->ssl_version = TLS1_2_VERSION;
  else if (strcasecmp(value, "tlsv1_3") == 0)
    self->ssl_version = TLS1_3_VERSION;
  else
    return FALSE;
  return TRUE;
#else
  msg_error("This version of syslog-ng was compiled against OpenSSL older than 1.1.0 which does not support ssl-version()");
  return FALSE;
#endif
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
#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
      else if (strcasecmp(l->data, "ignore-unexpected-eof") == 0 || strcasecmp(l->data, "ignore_unexpected_eof") == 0)
        self->ssl_options |= TSO_IGNORE_UNEXPECTED_EOF;
#endif
      else if (strcasecmp(l->data, "ignore-hostname-mismatch") == 0 || strcasecmp(l->data, "ignore_hostname_mismatch") == 0)
        self->ssl_options |= TSO_IGNORE_HOSTNAME_MISMATCH;
      else if (strcasecmp(l->data, "ignore-validity-period") == 0 || strcasecmp(l->data, "ignore_validity_period") == 0)
        self->ssl_options |= TSO_IGNORE_VALIDITY_PERIOD;
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

void
tls_context_set_verify_mode(TLSContext *self, gint verify_mode)
{
  self->verify_mode = verify_mode;
}

gboolean
tls_context_ignore_hostname_mismatch(TLSContext *self)
{
  return self->ssl_options & TSO_IGNORE_HOSTNAME_MISMATCH;
}

gboolean
tls_context_ignore_validity_period(TLSContext *self)
{
  return self->ssl_options & TSO_IGNORE_VALIDITY_PERIOD;
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
#if SYSLOG_NG_HAVE_DECL_SSL_CONF_CTX_NEW
  g_list_foreach(self->conf_cmds_list, (GFunc) g_free, NULL);
  self->conf_cmds_list = cmds;
  return TRUE;
#else
  g_set_error(error, TLSCONTEXT_ERROR, TLSCONTEXT_UNSUPPORTED,
              "Setting SSL conf context is not supported with the OpenSSL version syslog-ng was compiled with");
  return FALSE;
#endif
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
  SSL_CTX_set_app_data(self->ssl_ctx, self);

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
