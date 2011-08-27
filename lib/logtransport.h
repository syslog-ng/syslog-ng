/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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
#include "gsockaddr.h"

/* don't close the underlying fd when LogTransport is destructed */
#define LTF_DONTCLOSE 0x0001

/* issue an fsync after every write */
#define LTF_FSYNC     0x0002

/* reseek to the end-of-file before writing */
#define LTF_APPEND    0x0004

/* use recv() instead of read() */
#define LTF_RECV      0x0008

/* issue a shutdown() when LogTransport is destructed, only works if LTF_DONTCLOSE is unset */
#define LTF_SHUTDOWN  0x0010

/* the underlying fd is a pipe, try to resize the output chunk to make
 * sure it fits into PIPE_BUF, see then comment
 * log_transport_plain_write() for more details. */
#define LTF_PIPE      0x0020


typedef struct _LogTransport LogTransport;

struct _LogTransport
{
  gint fd;
  GIOCondition cond;
  guint flags;
  gint timeout;
  gssize (*read)(LogTransport *self, gpointer buf, gsize count, GSockAddr **sa);
  gssize (*write)(LogTransport *self, const gpointer buf, gsize count);
  void (*free_fn)(LogTransport *self);
};

static inline gssize 
log_transport_write(LogTransport *self, const gpointer buf, gsize count)
{
  return self->write(self, buf, count);
}

static inline gssize
log_transport_read(LogTransport *self, gpointer buf, gsize count, GSockAddr **sa)
{
  return self->read(self, buf, count, sa);
}

LogTransport *log_transport_plain_new(gint fd, guint flags);
void log_transport_free(LogTransport *s);
void log_transport_free_method(LogTransport *s);


#endif
