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

#include "transport-prockmsg.h"
#include "messages.h"
#include "alarms.h"

#include <errno.h>
#include <unistd.h>

typedef struct _LogTransportDevice LogTransportDevice;
struct _LogTransportDevice
{
  LogTransport super;
  gint timeout;
};

static gssize
log_transport_prockmsg_read_method(LogTransport *s, gpointer buf, gsize buflen, LogTransportAuxData *aux)
{
  LogTransportDevice *self = (LogTransportDevice *) s;
  gint rc;

  do
    {
      if (self->timeout)
        alarm_set(self->timeout);
      rc = read(self->super.fd, buf, buflen);

      if (self->timeout > 0 && rc == -1 && errno == EINTR && alarm_has_fired())
        {
          msg_notice("Nonblocking read has blocked, returning with an error",
                     evt_tag_int("fd", self->super.fd),
                     evt_tag_int("timeout", self->timeout));
          alarm_cancel();
          break;
        }
      if (self->timeout)
        alarm_cancel();
    }
  while (rc == -1 && errno == EINTR);
  return rc;
}

LogTransport *
log_transport_prockmsg_new(gint fd, gint timeout)
{
  LogTransportDevice *self = g_new0(LogTransportDevice, 1);

  log_transport_init_instance(&self->super, fd);
  self->timeout = timeout;
  self->super.read = log_transport_prockmsg_read_method;
  self->super.write = NULL;
  self->super.free_fn = log_transport_free_method;
  return &self->super;
}
