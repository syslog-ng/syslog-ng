/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#include "filter-netmask.h"
#include "gsocket.h"
#include "logmsg/logmsg.h"

#include <stdlib.h>
#include <string.h>

typedef struct _FilterNetmask
{
  FilterExprNode super;
  struct in_addr address;
  struct in_addr netmask;
} FilterNetmask;

static gboolean
filter_netmask_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterNetmask *self = (FilterNetmask *) s;
  struct in_addr *addr, addr_storage;
  LogMessage *msg = msgs[num_msg - 1];
  gboolean res;

  if (msg->saddr && g_sockaddr_inet_check(msg->saddr))
    {
      addr = &((struct sockaddr_in *) &msg->saddr->sa)->sin_addr;
    }
  else if (!msg->saddr || msg->saddr->sa.sa_family == AF_UNIX)
    {
      addr_storage.s_addr = htonl(INADDR_LOOPBACK);
      addr = &addr_storage;
    }
  else
    {
      addr = NULL;
    }

  if (addr)
    res = ((addr->s_addr & self->netmask.s_addr) == (self->address.s_addr));
  else
    res = FALSE;

  msg_debug("  netmask() evaluation result",
            filter_result_tag(res),
            evt_tag_inaddr("msg_address", addr),
            evt_tag_inaddr("address", &self->address),
            evt_tag_inaddr("netmask", &self->netmask),
            evt_tag_printf("msg", "%p", msg));
  return res ^ s->comp;
}

FilterExprNode *
filter_netmask_new(const gchar *cidr)
{
  FilterNetmask *self = g_new0(FilterNetmask, 1);
  gchar buf[32];
  gchar *slash;

  filter_expr_node_init_instance(&self->super);
  slash = strchr(cidr, '/');
  if (strlen(cidr) >= sizeof(buf) || !slash)
    {
      g_inet_aton(cidr, &self->address);
      self->netmask.s_addr = htonl(0xFFFFFFFF);
    }
  else
    {
      strncpy(buf, cidr, slash - cidr + 1);
      buf[slash - cidr] = 0;
      g_inet_aton(buf, &self->address);
      if (strchr(slash + 1, '.'))
        {
          g_inet_aton(slash + 1, &self->netmask);
        }
      else
        {
          gint prefix = strtol(slash + 1, NULL, 10);
          if (prefix == 32)
            self->netmask.s_addr = htonl(0xFFFFFFFF);
          else
            self->netmask.s_addr = htonl(((1 << prefix) - 1) << (32 - prefix));
        }
    }
  self->address.s_addr &= self->netmask.s_addr;
  self->super.eval = filter_netmask_eval;
  return &self->super;
}
