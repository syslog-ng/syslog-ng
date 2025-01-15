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

#include <fstream>

using namespace syslogng::cloud_auth::azure;

void AzureMonitorAuthenticator::handle_http_header_request(HttpHeaderRequestSignalData *data)
{
  ;
}

AzureMonitorAuthenticator::AzureMonitorAuthenticator()
{
  ;
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

static gboolean
_init(CloudAuthenticator *s)
{
  AzureAuthenticator *self = (AzureAuthenticator *) s;

  switch (self->auth_mode)
    {
    case AAAM_MONITOR:
      try
        {
          self->super.cpp = new AzureMonitorAuthenticator();
        }
      catch (const std::runtime_error &e)
        {
          msg_error("cloud_auth::azure: Failed to initialize AzureMonitorAuthenticator",
                    evt_tag_str("error", e.what()));
          return FALSE;
        }
      break;
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
