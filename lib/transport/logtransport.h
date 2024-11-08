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

/*
 * LogTransport:
 *
 * This is an interface that a LogProto implementation can use to do I/O.
 * There might be multiple LogTransport implementations alive for a specific
 * connection: for instance we might do both plain text and SSL encrypted
 * communication on the same socket, when the haproxy proxy protocol is in
 * use and SSL is enabled.  It might also make sense to instantiate a
 * transport doing gzip compression transparently.
 *
 * The combination of interoperating LogTransport instances is called the
 * LogTransportStack (see transport-stack.h)
 *
 * There's a circular, borrowed reference between the stack and the
 * constituent LogTransport instances.
 */

typedef struct _LogTransport LogTransport;
typedef struct _LogTransportStack LogTransportStack;
struct _LogTransport
{
  gint fd;
  GIOCondition cond;

  gssize (*read)(LogTransport *self, gpointer buf, gsize count, LogTransportAuxData *aux);
  gssize (*write)(LogTransport *self, const gpointer buf, gsize count);
  gssize (*writev)(LogTransport *self, struct iovec *iov, gint iov_count);
  void (*free_fn)(LogTransport *self);

  /* read ahead */
  struct
  {
    gchar buf[16];
    gint buf_len;
    gint pos;
  } ra;
  LogTransportStack *stack;
  const gchar *name;
};

static inline gboolean
log_transport_poll_prepare(LogTransport *self, GIOCondition *cond)
{
  if (self->ra.buf_len != self->ra.pos)
    return TRUE;
  *cond = self->cond;
  return FALSE;
}

static inline void
log_transport_assign_to_stack(LogTransport *self, LogTransportStack *stack)
{
  self->stack = stack;
}

static inline gssize
log_transport_write(LogTransport *self, const gpointer buf, gsize count)
{
  return self->write(self, buf, count);
}

static inline gssize
log_transport_writev(LogTransport *self, struct iovec *iov, gint iov_count)
{
  return self->writev(self, iov, iov_count);
}

gssize _log_transport_combined_read_with_read_ahead(LogTransport *self,
                                                    gpointer buf, gsize count,
                                                    LogTransportAuxData *aux);

static inline gssize
log_transport_read(LogTransport *self, gpointer buf, gsize count, LogTransportAuxData *aux)
{
  if (G_LIKELY(self->ra.buf_len == 0))
    return self->read(self, buf, count, aux);

  return _log_transport_combined_read_with_read_ahead(self, buf, count, aux);
}

gssize log_transport_read_ahead(LogTransport *self, gpointer buf, gsize count, gboolean *moved_forward);

void log_transport_init_instance(LogTransport *s, const gchar *name, gint fd);
void log_transport_free_method(LogTransport *s);
void log_transport_free(LogTransport *s);

#endif
