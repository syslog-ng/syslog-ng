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

#include <unistd.h>

void
log_transport_factory_init_instance(LogTransportFactory *self, LogTransportIndex index)
{
  memset(self, 0, sizeof(*self));
  self->index = index;
}

void
log_transport_stack_add_factory(LogTransportStack *self, LogTransportFactory *transport_factory)
{
  gint index = transport_factory->index;
  g_assert(self->transport_factories[index] == NULL);
  self->transport_factories[index] = transport_factory;
}

void
log_transport_stack_add_transport(LogTransportStack *self, gint index, LogTransport *transport)
{
  g_assert(self->transports[index] == NULL);
  log_transport_assign_to_stack(transport, self);
  self->transports[index] = transport;
  if (self->fd == -1)
    self->fd = transport->fd;
  else if (transport->fd != -1)
    g_assert(self->fd == transport->fd);
}

gboolean
log_transport_stack_switch(LogTransportStack *self, gint index)
{
  g_assert(index < LOG_TRANSPORT__MAX);
  LogTransport *active_transport = log_transport_stack_get_active(self);
  LogTransport *requested_transport = log_transport_stack_get_transport(self, index);

  g_assert(requested_transport != NULL);

  msg_debug("Transport switch requested",
            evt_tag_str("active-transport", active_transport ? active_transport->name : "none"),
            evt_tag_str("requested-transport", requested_transport->name));

  /* FIXME: is this cond initialization really needed? */
  if (active_transport)
    requested_transport->cond = active_transport->cond;
  self->active_transport = index;
  active_transport = log_transport_stack_get_active(self);

  msg_debug("Transport switch successful",
            evt_tag_str("new-active-transport", active_transport->name));

  return TRUE;
}

/*
 * Move the transport stack state to another LogTransportStack instance.
 * Normally LogTransportStack instances are embedded in LogProto instances,
 * so in case the LogProto instance is replaced, the transport stack may
 * need to be moved.
 */
void
log_transport_stack_move(LogTransportStack *self, LogTransportStack *other)
{
  self->fd = other->fd;
  self->active_transport = other->active_transport;
  other->fd = -1;

  for (gint i = 0; i < LOG_TRANSPORT__MAX; i++)
    {
      g_assert(self->transports[i] == NULL);
      g_assert(self->transport_factories[i] == NULL);

      if (other->transports[i])
        {
          self->transports[i] = other->transports[i];
          log_transport_assign_to_stack(self->transports[i], self);
          other->transports[i] = NULL;
        }
      if (other->transport_factories[i])
        {
          self->transport_factories[i] = other->transport_factories[i];
          other->transport_factories[i] = NULL;
        }
    }
}

void
log_transport_stack_init(LogTransportStack *self, LogTransport *initial_transport)
{
  memset(self, 0, sizeof(*self));
  self->fd = -1;
  if (initial_transport)
    log_transport_stack_add_transport(self, LOG_TRANSPORT_INITIAL, initial_transport);
}

void
log_transport_stack_deinit(LogTransportStack *self)
{
  if (self->fd != -1)
    {
      msg_trace("Closing log transport fd",
                evt_tag_int("fd", self->fd));
      close(self->fd);
    }
  for (gint i = 0; i < LOG_TRANSPORT__MAX; i++)
    {
      if (self->transports[i])
        log_transport_free(self->transports[i]);
      if (self->transport_factories[i])
        log_transport_factory_free(self->transport_factories[i]);
    }
}
