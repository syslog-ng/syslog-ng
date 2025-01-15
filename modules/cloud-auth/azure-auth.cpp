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

} _AzureAuthenticator;

static gboolean
_init(CloudAuthenticator *s)
{
  return TRUE;
}

static void
_free(CloudAuthenticator *s)
{
  ;
}

static void
_set_default_options(AzureAuthenticator *self)
{
  ;
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
