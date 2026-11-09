/*
 * Copyright (c) 2025 Axoflow
 * Copyright (c) 2025 Tamas Kosztyu <tamas.kosztyu@axoflow.com>
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
#include "azure-auth.hpp"

using namespace syslogng::cloud_auth::azure;
using namespace syslogng::cloud_auth::oauth2;

AzureMonitorAuthenticator::AzureMonitorAuthenticator(const char *tenant_id_,
                                                     const char *app_id_,
                                                     const char *app_secret_,
                                                     const char *scope_,
                                                     const char *cert_path_,
                                                     const char *private_key_path_,
                                                     const char *private_key_passphrase_)
  : OAuth2Authenticator(
      app_id_,                                                            // client_id
      (cert_path_ && *cert_path_) ? nullptr : app_secret_,                // client_secret (unused for cert auth)
      (std::string("https://login.microsoftonline.com/") + tenant_id_ +
       "/oauth2/v2.0/token").c_str(),                                    // token_url
      scope_,                                                             // scope
      nullptr,                                                           // resource (not used)
      nullptr,                                                           // authorization_details (not used)
      60,                                                                // refresh_offset (60s before expiry)
      (cert_path_ && *cert_path_) ? OAUTH2_AUTH_METHOD_PRIVATE_KEY_JWT
                                  : OAUTH2_AUTH_METHOD_POST_BODY,        // auth_method
      cert_path_,
      private_key_path_,
      private_key_passphrase_
    )
{
}

/* C Wrappers */

typedef struct AzureAuthenticator
{
  CloudAuthenticator super;

  AzureAuthenticatorAuthMode auth_mode;

  gchar *tenant_id;
  gchar *app_id;
  gchar *scope;
  gchar *app_secret;
  gchar *cert_path;
  gchar *private_key_path;
  gchar *private_key_passphrase;
} _AzureAuthenticator;

void
azure_authenticator_set_auth_mode(CloudAuthenticator *s, AzureAuthenticatorAuthMode auth_mode)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  self->auth_mode = auth_mode;
}

void
azure_authenticator_set_tenant_id(CloudAuthenticator *s, const gchar *tenant_id)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  g_free(self->tenant_id);
  self->tenant_id = g_strdup(tenant_id);
}

void
azure_authenticator_set_app_id(CloudAuthenticator *s, const gchar *app_id)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  g_free(self->app_id);
  self->app_id = g_strdup(app_id);
}

void
azure_authenticator_set_scope(CloudAuthenticator *s, const gchar *scope)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  g_free(self->scope);
  self->scope = g_strdup(scope);
}

void
azure_authenticator_set_app_secret(CloudAuthenticator *s, const gchar *app_secret)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  g_free(self->app_secret);
  self->app_secret = g_strdup(app_secret);
}

void
azure_authenticator_set_cert_path(CloudAuthenticator *s, const gchar *cert_path)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  g_free(self->cert_path);
  self->cert_path = g_strdup(cert_path);
}

void
azure_authenticator_set_private_key_path(CloudAuthenticator *s, const gchar *private_key_path)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  g_free(self->private_key_path);
  self->private_key_path = g_strdup(private_key_path);
}

void
azure_authenticator_set_private_key_passphrase(CloudAuthenticator *s, const gchar *private_key_passphrase)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  g_free(self->private_key_passphrase);
  self->private_key_passphrase = g_strdup(private_key_passphrase);
}

static gboolean
_init(CloudAuthenticator *s)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  switch (self->auth_mode)
    {
    case AAAM_MONITOR:
      {
        gboolean has_secret = self->app_secret && self->app_secret[0];
        gboolean has_cert = self->cert_path && self->cert_path[0];
        gboolean has_key = self->private_key_path && self->private_key_path[0];

        if (has_cert != has_key)
          {
            msg_error("cloud_auth::azure: Failed to initialize AzureMonitorAuthenticator",
                      evt_tag_str("error", "cert() and private_key() must be set together"));
            return FALSE;
          }
        if (has_secret == has_cert)
          {
            msg_error("cloud_auth::azure: Failed to initialize AzureMonitorAuthenticator",
                      evt_tag_str("error", "exactly one of app_secret() or cert()+private_key() must be set"));
            return FALSE;
          }

        try
          {
            self->super.cpp = new AzureMonitorAuthenticator(self->tenant_id,
                                                            self->app_id,
                                                            self->app_secret,
                                                            self->scope,
                                                            self->cert_path,
                                                            self->private_key_path,
                                                            self->private_key_passphrase);
          }
        catch (const std::runtime_error &e)
          {
            msg_error("cloud_auth::azure: Failed to initialize AzureMonitorAuthenticator",
                      evt_tag_str("error", e.what()));
            return FALSE;
          }
        break;
      }
    case AAAM_UNDEFINED:
      msg_error("cloud_auth::azure: Failed to initialize AzureMonitorAuthenticator",
                evt_tag_str("error", "Authentication mode must be set (e.g. monitor())"));
      return FALSE;
    default:
      g_assert_not_reached();
    }

  return TRUE;
}

static void
_free(CloudAuthenticator *s)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  g_free(self->tenant_id);
  g_free(self->app_id);
  g_free(self->scope);
  g_free(self->app_secret);
  g_free(self->cert_path);
  g_free(self->private_key_path);
  g_free(self->private_key_passphrase);
}

static void
_set_default_options(AzureAuthenticator *self)
{
  self->scope = g_strdup("https://monitor.azure.com//.default");
}

CloudAuthenticator *
azure_authenticator_new(void)
{
  AzureAuthenticator *self = g_new0(AzureAuthenticator, 1);

  self->super.init = _init;
  self->super.free_fn = _free;

  _set_default_options(self);

  return &self->super;
}
