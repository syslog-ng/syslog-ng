/*
 * Copyright (c) 2023 Attila Szakacs
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

#include "cloud-auth.h"
#include "signal-slot-connector/signal-slot-connector.h"

#define CLOUD_AUTH_PLUGIN_NAME "cloud_auth"

struct _CloudAuthDestPlugin
{
  LogDriverPlugin super;
  CloudAuthenticator *authenticator;
};

static gboolean
_attach(LogDriverPlugin *s, LogDriver *d)
{
  CloudAuthDestPlugin *self = (CloudAuthDestPlugin *) s;
  LogDestDriver *driver = (LogDestDriver *) d;

  if (!cloud_authenticator_init(self->authenticator))
    return FALSE;

  SignalSlotConnector *ssc = driver->super.super.signal_slot_connector;
  CONNECT(ssc, signal_http_header_request, cloud_authenticator_handle_http_header_request, self->authenticator);

  return TRUE;
}

static void
_detach(LogDriverPlugin *s, LogDriver *d)
{
  CloudAuthDestPlugin *self = (CloudAuthDestPlugin *) s;
  LogDestDriver *driver = (LogDestDriver *) d;

  cloud_authenticator_deinit(self->authenticator);

  SignalSlotConnector *ssc = driver->super.super.signal_slot_connector;
  DISCONNECT(ssc, signal_http_header_request, cloud_authenticator_handle_http_header_request, self->authenticator);
}

void
cloud_auth_dest_plugin_set_authenticator(LogDriverPlugin *s, CloudAuthenticator *authenticator)
{
  CloudAuthDestPlugin *self = (CloudAuthDestPlugin *) s;

  if (self->authenticator)
    cloud_authenticator_free(self->authenticator);
  self->authenticator = authenticator;
}

static void
_free(LogDriverPlugin *s)
{
  CloudAuthDestPlugin *self = (CloudAuthDestPlugin *) s;

  if (self->authenticator)
    cloud_authenticator_free(self->authenticator);
  log_driver_plugin_free_method(s);
}

LogDriverPlugin *
cloud_auth_dest_plugin_new(void)
{
  CloudAuthDestPlugin *self = g_new0(CloudAuthDestPlugin, 1);

  log_driver_plugin_init_instance(&self->super, CLOUD_AUTH_PLUGIN_NAME);
  self->super.attach = _attach;
  self->super.detach = _detach;
  self->super.free_fn = _free;

  return &self->super;
}
