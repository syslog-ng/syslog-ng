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

#include "transport/transport-stack.h"
#include "messages.h"

void
log_transport_stack_add_factory(LogTransportStack *self, LogTransportFactory *transport_factory)
{
  g_hash_table_insert(self->registry, (gpointer) transport_factory->id, transport_factory);
}

static void
_do_transport_switch(LogTransportStack *self, LogTransport *new_transport, const LogTransportFactory *new_transport_factory)
{
  self->super.fd = log_transport_release_fd(self->active_transport);
  self->super.cond = new_transport->cond;
  /* At this point the proxy of the active transport must be released, and would be nice to handle the ownership passing here, but
   *   - the proxy is not in the LogTransport, it is socket only now, and
   *   - multitransport is a generic class, can handle none LogTransportSocket based transports
   */
  log_transport_free(self->active_transport);
  self->active_transport = new_transport;
  self->active_transport_factory = new_transport_factory;
}

static const LogTransportFactory *
_lookup_transport_factory(LogTransportStack *self, const gchar *factory_id)
{
  const LogTransportFactory *factory = g_hash_table_lookup(self->registry, factory_id);

  if (!factory)
    {
      msg_error("Requested transport not found",
                evt_tag_str("transport", factory_id));
      return NULL;
    }

  return factory;
}

static LogTransport *
_construct_transport(const LogTransportFactory *factory, gint fd)
{
  LogTransport *transport = log_transport_factory_construct_transport(factory, fd);

  if (!transport)
    {
      msg_error("Failed to construct transport",
                evt_tag_str("transport", factory->id));
      return NULL;
    }

  return transport;
}

gboolean
log_transport_stack_switch(LogTransportStack *self, const gchar *factory_id)
{
  msg_debug("Transport switch requested",
            evt_tag_str("active-transport", self->active_transport->name),
            evt_tag_str("requested-transport", factory_id));

  const LogTransportFactory *transport_factory = _lookup_transport_factory(self, factory_id);
  if (!transport_factory)
    return FALSE;

  LogTransport *transport = _construct_transport(transport_factory, self->super.fd);
  if (!transport)
    return FALSE;

  _do_transport_switch(self, transport, transport_factory);

  msg_debug("Transport switch succeeded",
            evt_tag_str("new-active-transport", self->active_transport->name));

  return TRUE;
}

gboolean
log_transport_stack_contains_factory(LogTransportStack *self, const gchar *factory_id)
{
  const LogTransportFactory *factory = g_hash_table_lookup(self->registry, factory_id);

  return (factory != NULL);
}

static gssize
_log_transport_stack_write(LogTransport *s, gpointer buf, gsize count)
{
  LogTransportStack *self = (LogTransportStack *)s;
  gssize r = log_transport_write(self->active_transport, buf, count);
  self->super.cond = self->active_transport->cond;

  return r;
}

static gssize
_log_transport_stack_read(LogTransport *s, gpointer buf, gsize count, LogTransportAuxData *aux)
{
  LogTransportStack *self = (LogTransportStack *)s;
  gssize r = log_transport_read(self->active_transport, buf, count, aux);
  self->super.cond = self->active_transport->cond;

  return r;
}

static void
_log_transport_stack_free(LogTransport *s)
{
  LogTransportStack *self = (LogTransportStack *)s;
  s->fd = log_transport_release_fd(self->active_transport);
  log_transport_free(self->active_transport);
  g_hash_table_unref(self->registry);
  log_transport_free_method(s);
}

LogTransport *
log_transport_stack_new(LogTransportFactory *default_transport_factory, gint fd)
{
  LogTransportStack *self = g_new0(LogTransportStack, 1);

  log_transport_init_instance(&self->super, "transport_stack", fd);
  self->super.read = _log_transport_stack_read;
  self->super.write = _log_transport_stack_write;
  self->super.free_fn = _log_transport_stack_free;
  self->registry = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, (GDestroyNotify) log_transport_factory_free);
  self->active_transport = log_transport_factory_construct_transport(default_transport_factory, fd);
  self->active_transport_factory = default_transport_factory;

  return &self->super;
}
