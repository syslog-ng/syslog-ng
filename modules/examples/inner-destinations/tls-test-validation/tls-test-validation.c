/*
 * Copyright (c) 2023 Ricardo Filipe <ricardo.l.filipe@tecnico.ulisboa.pt>
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

#include "tls-test-validation.h"
#include "modules/afsocket/afsocket-signals.h"
#include "transport/tls-context.h"
#include "compat/openssl_support.h"

#define TLS_TEST_VALIDATION_PLUGIN "tls-test-validation"

struct _TlsTestValidationPlugin
{
  LogDriverPlugin super;
  gchar *identity;
};

void
tls_test_validation_plugin_set_identity(TlsTestValidationPlugin *self, const gchar *identity)
{
  g_free(self->identity);
  self->identity = g_strdup(identity);
}

static void
_slot_append_test_identity(TlsTestValidationPlugin *self, AFSocketTLSCertificateValidationSignalData *data)
{
  X509 *cert = X509_STORE_CTX_get0_cert(data->ctx);
  data->failure = !tls_context_verify_peer(data->tls_context, cert, self->identity);

  msg_debug("TlsTestValidationPlugin validated");
}

static gboolean
_attach(LogDriverPlugin *s, LogDriver *driver)
{
  SignalSlotConnector *ssc = driver->super.signal_slot_connector;

  msg_debug("TlsTestValidationPlugin::attach()",
            evt_tag_printf("SignalSlotConnector", "%p", ssc));

  CONNECT(ssc, signal_afsocket_tls_certificate_validation, _slot_append_test_identity, s);

  return TRUE;
}

static void
_detach(LogDriverPlugin *s, LogDriver *driver)
{
  SignalSlotConnector *ssc = driver->super.signal_slot_connector;

  msg_debug("TlsTestValidationPlugin::detach()",
            evt_tag_printf("SignalSlotConnector", "%p", ssc));

  DISCONNECT(ssc, signal_afsocket_tls_certificate_validation, _slot_append_test_identity, s);
}

static void
_free(LogDriverPlugin *s)
{
  msg_debug("TlsTestValidationPlugin::free");
  TlsTestValidationPlugin *self = (TlsTestValidationPlugin *)s;
  g_free(self->identity);
  log_driver_plugin_free_method(s);
}

TlsTestValidationPlugin *
tls_test_validation_plugin_new(void)
{
  TlsTestValidationPlugin *self = g_new0(TlsTestValidationPlugin, 1);

  log_driver_plugin_init_instance(&self->super, TLS_TEST_VALIDATION_PLUGIN);

  self->super.attach = _attach;
  self->super.detach = _detach;
  self->super.free_fn = _free;

  return self;
}
