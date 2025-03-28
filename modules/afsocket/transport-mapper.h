/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#ifndef TRANSPORT_MAPPER_H_INCLUDED
#define TRANSPORT_MAPPER_H_INCLUDED

#include "socket-options.h"
#include "transport/transport-stack.h"
#include "gsockaddr.h"

typedef struct _TransportMapper TransportMapper;
typedef gboolean (*TransportMapperAsyncInitCB)(gpointer arg);

struct _TransportMapper
{
  /* the transport() option as specified by the user */
  gchar *transport;

  /* output parameters as determinted by TransportMapper */
  gint address_family;
  /* SOCK_DGRAM or SOCK_STREAM or other SOCK_XXX values used by the socket() call */
  gint sock_type;
  /* protocol parameter for the socket() call, 0 for default or IPPROTO_XXX for specific transports */
  gint sock_proto;

  const gchar *logproto;
  /* the user visible summary of the transport, to be put into $TRANSPORT */
  gchar *transport_name;
  gsize transport_name_len;
  gint stats_source;

  gboolean (*apply_transport)(TransportMapper *self, GlobalConfig *cfg);
  gboolean (*setup_stack)(TransportMapper *self, LogTransportStack *stack);
  gboolean (*init)(TransportMapper *self);
  gboolean (*async_init)(TransportMapper *self, TransportMapperAsyncInitCB func, gpointer arg);
  void (*free_fn)(TransportMapper *self);
};

void transport_mapper_set_transport(TransportMapper *self, const gchar *transport);
void transport_mapper_set_address_family(TransportMapper *self, gint address_family);

gboolean transport_mapper_open_socket(TransportMapper *self,
                                      SocketOptions *socket_options,
                                      GSockAddr *bind_addr,
                                      GSockAddr *peer_addr,
                                      AFSocketDirection dir,
                                      int *fd);

gboolean transport_mapper_apply_transport_method(TransportMapper *self, GlobalConfig *cfg);

void transport_mapper_init_instance(TransportMapper *self, const gchar *transport);
void transport_mapper_free(TransportMapper *self);
void transport_mapper_free_method(TransportMapper *self);

static inline const gchar *
transport_mapper_get_transport_name(TransportMapper *self, gsize *len)
{
  if (self->transport_name)
    {
      if (len)
        *len = self->transport_name_len;
      return self->transport_name;
    }
  return NULL;
}

static inline gboolean
transport_mapper_apply_transport(TransportMapper *self, GlobalConfig *cfg)
{
  gboolean result = self->apply_transport(self, cfg);

  if (result)
    self->transport_name_len = self->transport_name ? strlen(self->transport_name) : 0;
  return result;
}

static inline gboolean
transport_mapper_setup_stack(TransportMapper *self, LogTransportStack *stack, gint fd)
{
  stack->fd = fd;
  return self->setup_stack(self, stack);
}

static inline gboolean
transport_mapper_init(TransportMapper *self)
{
  if (self->init)
    return self->init(self);

  return TRUE;
}

static inline gboolean
transport_mapper_async_init(TransportMapper *self, TransportMapperAsyncInitCB func, gpointer arg)
{
  if (self->async_init)
    {
      return self->async_init(self, func, arg);
    }

  return func(arg);
}
#endif
