/*
 * Copyright (c) 2021 One Identity
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "mqtt-options.h"
#include "syslog-ng.h"

#include <MQTTClient.h>
#include <string.h>
#include <stddef.h>


#define DEFAULT_ADDRESS "tcp://localhost:1883"
#define DEFAULT_KEEPALIVE 60
#define DEFAULT_QOS 0



static gboolean
_validate_protocol(const gchar *address)
{
  const gchar *valid_type[] = {"tcp", "ssl", "ws", "wss"};
  gint i;

  for (i = 0; i < G_N_ELEMENTS(valid_type); ++i)
    if (strncmp(valid_type[i], address, strlen(valid_type[i])) == 0)
      return TRUE;

  return FALSE;
}

static gboolean
_validate_address(const gchar *address)
{
  if (strstr(address, "://") == NULL)
    return FALSE;

  if (!_validate_protocol(address))
    return FALSE;

  return TRUE;
}

void
mqtt_client_options_defaults(MQTTClientOptions *self)
{
  self->address = g_strdup(DEFAULT_ADDRESS);
  self->keepalive = DEFAULT_KEEPALIVE;
  self->qos = DEFAULT_QOS;

  self->ssl_version = MQTT_SSL_VERSION_DEFAULT;
  self->peer_verify = TRUE;
  self->use_system_cert_store = FALSE;
}

void
mqtt_client_options_destroy(MQTTClientOptions *self)
{
  g_free(self->address);
  g_free(self->client_id);

  g_free(self->username);
  g_free(self->password);
  g_free(self->http_proxy);

  g_free(self->ca_dir);
  g_free(self->ca_file);
  g_free(self->cert_file);
  g_free(self->key_file);
  g_free(self->ciphers);
}

void
mqtt_client_options_set_keepalive(MQTTClientOptions *self, const gint keepalive)
{
  self->keepalive = keepalive;
}

gboolean
mqtt_client_options_set_address(MQTTClientOptions *self, const gchar *address)
{
  if (!_validate_address(address))
    return FALSE;

  g_free(self->address);
  self->address = g_strdup(address);
  return TRUE;
}

void
mqtt_client_options_set_qos (MQTTClientOptions *self, const gint qos)
{
  self->qos = qos;
}

gboolean
mqtt_client_options_set_client_id(MQTTClientOptions *self, const gchar *client_id)
{
  if(strcmp("", client_id) == 0)
    return FALSE;

  g_free(self->client_id);
  self->client_id = g_strdup(client_id);

  return TRUE;
}

void
mqtt_client_options_set_cleansession(MQTTClientOptions *self, gboolean cleansession)
{
  self->cleansession = cleansession;
}

void
mqtt_client_options_set_username(MQTTClientOptions *self, const gchar *username)
{
  g_free(self->username);
  self->username = g_strdup(username);
}

void
mqtt_client_options_set_password(MQTTClientOptions *self, const gchar *password)
{
  g_free(self->password);
  self->password = g_strdup(password);
}

void
mqtt_client_options_set_http_proxy(MQTTClientOptions *self, const gchar *http_proxy)
{
  g_free(self->http_proxy);
  self->http_proxy = g_strdup(http_proxy);
}


void
mqtt_client_options_set_ca_dir(MQTTClientOptions *self, const gchar *ca_dir)
{
  g_free(self->ca_dir);
  self->ca_dir = g_strdup(ca_dir);
}

void
mqtt_client_options_set_ca_file(MQTTClientOptions *self, const gchar *ca_file)
{
  g_free(self->ca_file);
  self->ca_file = g_strdup(ca_file);
}

void
mqtt_client_options_set_cert_file(MQTTClientOptions *self, const gchar *cert_file)
{
  g_free(self->cert_file);
  self->cert_file = g_strdup(cert_file);
}

void
mqtt_client_options_set_key_file(MQTTClientOptions *self, const gchar *key_file)
{
  g_free(self->key_file);
  self->key_file = g_strdup(key_file);
}

void
mqtt_client_options_set_cipher_suite(MQTTClientOptions *self, const gchar *ciphers)
{
  g_free(self->ciphers);
  self->ciphers = g_strdup(ciphers);
}

gboolean
mqtt_client_options_set_ssl_version(MQTTClientOptions *self, const gchar *value)
{
  if (strcasecmp(value, "default") == 0)
    self->ssl_version = MQTT_SSL_VERSION_DEFAULT;
  else if (strcasecmp(value, "tlsv1_0") == 0)
    self->ssl_version = MQTT_SSL_VERSION_TLS_1_0;
  else if (strcasecmp(value, "tlsv1_1") == 0)
    self->ssl_version = MQTT_SSL_VERSION_TLS_1_1;
  else if (strcasecmp(value, "tlsv1_2") == 0)
    self->ssl_version = MQTT_SSL_VERSION_TLS_1_2;
  else
    return FALSE;

  return TRUE;
}

void
mqtt_client_options_set_peer_verify(MQTTClientOptions *self, gboolean verify)
{
  self->peer_verify = verify;
}

void
mqtt_client_options_use_system_cert_store(MQTTClientOptions *self, gboolean use_system_cert_store)
{
  /* TODO: auto_detect_ca_file() from the HTTP module */
  self->use_system_cert_store = use_system_cert_store;
}

void
mqtt_client_options_set_log_ssl_error_fn(MQTTClientOptions *self, gpointer context, gint(*log_error)(const gchar *str,
                                         gsize len, gpointer u))
{
  self->context = context;
  self->log_error = log_error;
}

gchar *
mqtt_client_options_get_address(MQTTClientOptions *self)
{
  return self->address;
}

gint
mqtt_client_options_get_qos(MQTTClientOptions *self)
{
  return self->qos;
}

gchar *
mqtt_client_options_get_client_id(MQTTClientOptions *self)
{
  return self->client_id;
}

static void
_set_ssl_options(MQTTClientOptions *self, MQTTClient_SSLOptions *ssl_opts)
{
  *ssl_opts = (MQTTClient_SSLOptions) MQTTClient_SSLOptions_initializer;
  ssl_opts->trustStore = self->ca_file;
  ssl_opts->CApath = self->ca_dir;
  ssl_opts->keyStore = self->cert_file;
  ssl_opts->privateKey = self->key_file;
  ssl_opts->enabledCipherSuites = self->ciphers;
  ssl_opts->sslVersion = self->ssl_version;
  ssl_opts->enableServerCertAuth = self->peer_verify;
  ssl_opts->verify = self->peer_verify;
  ssl_opts->disableDefaultTrustStore = !self->use_system_cert_store;
  ssl_opts->ssl_error_cb = self->log_error;
  ssl_opts->ssl_error_context = self->context;
}

void
mqtt_client_options_to_mqtt_client_connection_option(MQTTClientOptions *self, MQTTClient_connectOptions *conn_opts,
                                                     MQTTClient_SSLOptions *ssl_opts)
{
  *conn_opts = (MQTTClient_connectOptions) MQTTClient_connectOptions_initializer;
  conn_opts->keepAliveInterval = self->keepalive;
  conn_opts->cleansession = self->cleansession;
  conn_opts->username = self->username;
  conn_opts->password = self->password;

#if SYSLOG_NG_HAVE_PAHO_HTTP_PROXY
  if (self->http_proxy)
    {
      conn_opts->httpProxy = self->http_proxy;
      conn_opts->httpsProxy = self->http_proxy;
    }
#endif

  _set_ssl_options(self, ssl_opts);
  conn_opts->ssl = ssl_opts;
}

static gboolean
_key_or_cert_file_is_not_specified(MQTTClientOptions *self)
{
  return (!self->key_file || !self->cert_file);
}

static gboolean
_is_using_tls(MQTTClientOptions *self)
{
  return (strncmp("ssl", self->address, 3) == 0) ||
         (strncmp("wss", self->address, 3) == 0);
}

gboolean
mqtt_client_options_checker(MQTTClientOptions *self)
{

#if !SYSLOG_NG_HAVE_PAHO_HTTP_PROXY
  if (self->http_proxy)
    {
      msg_warning_once("WARNING: the http-proxy() option of the mqtt() destination "
                       "is not supported on the current libpaho-mqtt version. "
                       "If you would like to use this feature, update to at least libpaho-mqtt 1.3.7");
      g_free(self->http_proxy);
      self->http_proxy = NULL;
    }
#endif

  if (_is_using_tls(self) && _key_or_cert_file_is_not_specified(self))
    {
      msg_warning("MQTT: You have a TLS enabled without a X.509 keypair."
                  " Make sure you have tls(key-file() and cert-file()) options, "
                  "TLS handshake to this source will fail");
    }

  return TRUE;
}
