/*
 * Copyright (c) 2002-2007 BalaBit IT Ltd, Budapest, Hungary                    
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
  
#ifndef FDWRITE_H_INCLUDED
#define FDWRITE_H_INCLUDED

#include "syslog-ng.h"
#include <unistd.h>

typedef struct _FDWrite FDWrite;

struct _FDWrite
{
  gint fd;
  GIOCondition cond;
  gboolean fsync;
  gint timeout;
  size_t (*write)(FDWrite *self, const void *buf, size_t count);
  void (*free_fn)(FDWrite *self);
};

static inline ssize_t 
fd_write(FDWrite *s, const void *buf, size_t count)
{
  return s->write(s, buf, count);
}

FDWrite *fd_write_new(gint fd);
void fd_write_free(FDWrite *self);

void fd_write_free_method(FDWrite *self);

#endif
