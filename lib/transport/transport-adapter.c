/*
 * Copyright (c) 2024 Balazs Scheidler <balazs.scheidler@axoflow.com>
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

#include "transport-adapter.h"

gssize
log_transport_adapter_read_method(LogTransport *s, gpointer buf, gsize buflen, LogTransportAuxData *aux)
{
  LogTransportAdapter *self = (LogTransportAdapter *) s;
  LogTransport *transport = log_transport_stack_get_transport(s->stack, self->base_index);

  return log_transport_read(transport, buf, buflen, aux);
}

gssize
log_transport_adapter_write_method(LogTransport *s, const gpointer buf, gsize count)
{
  LogTransportAdapter *self = (LogTransportAdapter *) s;
  LogTransport *transport = log_transport_stack_get_transport(s->stack, self->base_index);

  return log_transport_write(transport, buf, count);
}

gssize
log_transport_adapter_writev_method(LogTransport *s, struct iovec *iov, gint iov_count)
{
  LogTransportAdapter *self = (LogTransportAdapter *) s;
  LogTransport *transport = log_transport_stack_get_transport(s->stack, self->base_index);

  return log_transport_writev(transport, iov, iov_count);
}

void
log_transport_adapter_init_instance(LogTransportAdapter *self, const gchar *name,
                                    LogTransportIndex base_index)
{
  log_transport_init_instance(&self->super, name, -1);
  self->super.read = log_transport_adapter_read_method;
  self->super.write = log_transport_adapter_write_method;
  self->super.writev = log_transport_adapter_writev_method;

  self->base_index = base_index;
}
