/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
 *
 */


#include "logproto-client.h"
#include "messages.h"
#include "cfg.h"
#include "plugin.h"
#include "plugin-types.h"

gboolean
log_proto_client_validate_options_method(LogProtoClient *s)
{
  return TRUE;
}

void
log_proto_client_free_method(LogProtoClient *s)
{
  log_transport_stack_deinit(&s->transport_stack);
}

void
log_proto_client_free(LogProtoClient *s)
{
  if (s->free_fn)
    s->free_fn(s);
  g_free(s);
}

void
log_proto_client_init(LogProtoClient *self, LogTransport *transport, const LogProtoClientOptions *options)
{
  self->validate_options = log_proto_client_validate_options_method;
  self->free_fn = log_proto_client_free_method;
  self->options = options;
  log_transport_stack_init(&self->transport_stack, transport);
}

void
log_proto_client_options_set_drop_input(LogProtoClientOptions *options, gboolean drop_input)
{
  options->drop_input = drop_input;
}

void
log_proto_client_options_set_timeout(LogProtoClientOptions *options, gint timeout)
{
  options->timeout = timeout;
}

gint
log_proto_client_options_get_timeout(LogProtoClientOptions *options)
{
  return options->timeout;
}

void
log_proto_client_options_defaults(LogProtoClientOptions *options)
{
  options->drop_input = FALSE;
  options->timeout = 0;
}

void
log_proto_client_options_init(LogProtoClientOptions *options, GlobalConfig *cfg)
{
}

void
log_proto_client_options_destroy(LogProtoClientOptions *options)
{
}

LogProtoClientFactory *
log_proto_client_get_factory(PluginContext *context, const gchar *name)
{
  Plugin *plugin;

  plugin = plugin_find(context, LL_CONTEXT_CLIENT_PROTO, name);
  if (plugin && plugin->construct)
    return plugin->construct(plugin);

  return NULL;
}
