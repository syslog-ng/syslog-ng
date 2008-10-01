/*
 * COPYRIGHTHERE
 */
  
#ifndef TLSCONTEXT_H_INCLUDED
#define TLSCONTEXT_H_INCLUDED

#include "syslog-ng.h"

#ifdef ENABLE_SSL

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

#define tls_context_new(m)

#endif

#endif
