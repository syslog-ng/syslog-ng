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

#ifndef LOGTRANSPORT_H_INCLUDED
#define LOGTRANSPORT_H_INCLUDED

#include "syslog-ng.h"
#include "transport/transport-aux-data.h"

typedef struct _LogTransport LogTransport;

struct _LogTransport
{
  gint fd;
  GIOCondition cond;
  gssize (*read)(LogTransport *self, gpointer buf, gsize count, LogTransportAuxData *aux);
  gssize (*write)(LogTransport *self, const gpointer buf, gsize count);
  gint (*get_id)(LogTransport *self);
  void (*free_fn)(LogTransport *self);
};

static inline gssize
log_transport_write(LogTransport *self, const gpointer buf, gsize count)
{
  return self->write(self, buf, count);
}

static inline gssize
log_transport_read(LogTransport *self, gpointer buf, gsize count, LogTransportAuxData *aux)
{
  return self->read(self, buf, count, aux);
}

static inline gint
log_transport_get_id(LogTransport *self)
{
  if (self->get_id)
    return self->get_id(self);

  return -1;
}

void log_transport_init_instance(LogTransport *s, gint fd);
void log_transport_free_method(LogTransport *s);
void log_transport_free(LogTransport *s);

#endif
