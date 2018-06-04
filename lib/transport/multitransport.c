/*
 * Copyright (c) 2002-2018 Balabit
 * Copyright (c) 2018 Laszlo Budai <laszlo.budai@balabit.com>
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

#include "transport/multitransport.h"
#include "transport/transport-factory-registry.h"
#include "messages.h"

void multitransport_add_factory(MultiTransport *self, TransportFactory *transport_factory)
{
  transport_factory_registry_add(self->registry, transport_factory);
}

static void
_do_transport_switch(MultiTransport *self, LogTransport *new_transport, const TransportFactory *new_transport_factory)
{
  self->super.fd = log_transport_release_fd(self->active_transport);
  self->super.cond = new_transport->cond;
  log_transport_free(self->active_transport);
  self->active_transport = new_transport;
  self->active_transport_factory = new_transport_factory;
}

static const TransportFactory *
_lookup_transport_factory(TransportFactoryRegistry *registry, const TransportFactoryId *factory_id)
{
  const TransportFactory *factory = transport_factory_registry_lookup(registry, factory_id);

  if (!factory)
    {
      msg_error("Requested transport not found",
                evt_tag_str("transport", transport_factory_id_get_transport_name(factory_id)));
      return NULL;
    }

  return factory;
}

static LogTransport *
_construct_transport(const TransportFactory *factory, gint fd)
{
  LogTransport *transport = transport_factory_construct_transport(factory, fd);
  const TransportFactoryId *factory_id = transport_factory_get_id(factory);

  if (!transport)
    {
      msg_error("Failed to construct transport",
                evt_tag_str("transport", transport_factory_id_get_transport_name(factory_id)));
      return NULL;
    }

  return transport;
}

gboolean multitransport_switch(MultiTransport *self, const TransportFactoryId *factory_id)
{
  msg_debug("Transport switch requested",
            evt_tag_str("active-transport", self->active_transport->name),
            evt_tag_str("requested-transport", transport_factory_id_get_transport_name(factory_id)));

  const TransportFactory *transport_factory = _lookup_transport_factory(self->registry, factory_id);
  if (!transport_factory)
    return FALSE;

  LogTransport *transport = _construct_transport(transport_factory, self->super.fd);
  if (!transport)
    return FALSE;

  _do_transport_switch(self, transport, transport_factory);

  msg_debug("Transport switch succeded",
            evt_tag_str("new-active-transport", self->active_transport->name));

  return TRUE;
}

gboolean
multitransport_contains_factory(MultiTransport *self, const TransportFactoryId *factory_id)
{
  const TransportFactory *factory = _lookup_transport_factory(self->registry, factory_id);

  return (factory != NULL);
}

static gssize
_multitransport_write(LogTransport *s, gpointer buf, gsize count)
{
  MultiTransport *self = (MultiTransport *)s;
  gssize r = log_transport_write(self->active_transport, buf, count);
  self->super.cond = self->active_transport->cond;

  return r;
}

static gssize
_multitransport_read(LogTransport *s, gpointer buf, gsize count, LogTransportAuxData *aux)
{
  MultiTransport *self = (MultiTransport *)s;
  gssize r = log_transport_read(self->active_transport, buf, count, aux);
  self->super.cond = self->active_transport->cond;

  return r;
}

static void
_multitransport_free(LogTransport *s)
{
  MultiTransport *self = (MultiTransport *)s;
  s->fd = log_transport_release_fd(self->active_transport);
  log_transport_free(self->active_transport);
  transport_factory_registry_free(self->registry);
  log_transport_free_method(s);
}

LogTransport *
multitransport_new(TransportFactory *default_transport_factory, gint fd)
{
  MultiTransport *self = g_new0(MultiTransport, 1);
  self->registry = transport_factory_registry_new();
  transport_factory_registry_add(self->registry, default_transport_factory);

  log_transport_init_instance(&self->super, fd);
  self->super.read = _multitransport_read;
  self->super.write = _multitransport_write;
  self->super.free_fn = _multitransport_free;
  self->active_transport = transport_factory_construct_transport(default_transport_factory, fd);
  self->active_transport_factory = default_transport_factory;

  return &self->super;
}
