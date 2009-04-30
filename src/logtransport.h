/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef LOGTRANSPORT_H_INCLUDED
#define LOGTRANSPORT_H_INCLUDED

#include "syslog-ng.h"
#include "gsockaddr.h"

#define LTF_DONTCLOSE 0x0001
#define LTF_FSYNC     0x0002
#define LTF_APPEND    0x0004
#define LTF_RECV      0x0008
#define LTF_SHUTDOWN  0x0010


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
