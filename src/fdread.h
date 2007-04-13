/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
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

#ifndef FDREAD_H_INCLUDED
#define FDREAD_H_INCLUDED

#include "syslog-ng.h"
#include "gsockaddr.h"

#include <unistd.h>

#define FR_DONTCLOSE 0x0001
#define FR_RECV      0x0002

typedef struct _FDRead
{
  gint fd;
  GIOCondition cond;
  guint flags;
  size_t (*read)(struct _FDRead *, void *buf, size_t count, GSockAddr **sa);
} FDRead;

static inline ssize_t 
fd_read(FDRead *s, void *buf, size_t count, GSockAddr **sa)
{
  return s->read(s, buf, count, sa);
}

FDRead *fd_read_new(gint fd, guint flags);
void fd_read_free(FDRead *self);

#endif
