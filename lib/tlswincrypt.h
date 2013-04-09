#ifndef TLSWINCRYPT_H
#define TLSWINCRYPT_H 1

#include "messages.h"
#include "syslog-ng.h"
#include <openssl/ssl.h>
#include <openssl/engine.h>
#include <openssl/conf.h>
#include <glib.h>

#ifdef _WIN32
gboolean load_certificate(SSL_CTX *ctx,const gchar *cert_subject);
gboolean load_all_trusted_ca_certificates(SSL_CTX *ctx);
gboolean load_all_crls(SSL_CTX *ctx);
#else
static inline gboolean init_windows_crypt(SSL_CTX *ctx, const gchar *cert_subject)
{
  msg_error("This function shouldn't be called under non-Windows platform",evt_tag_str("function_name",__FUNCTION__));
  abort();
}

gboolean load_all_trusted_ca_certificates(SSL_CTX *ctx)
{
  msg_error("This function shouldn't be called under non-Windows platform",evt_tag_str("function_name",__FUNCTION__));
  abort();
}

gboolean  load_all_crls(SSL_CTX *ctx)
{
  msg_error("This function shouldn't be called under non-Windows platform",evt_tag_str("function_name",__FUNCTION__));
  abort();
}
#endif
#endif
