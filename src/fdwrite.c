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
  
#include "fdwrite.h"
#include "messages.h"
#include "alarms.h"

#include <errno.h>

/* FIXME: should create a separate abstract class */

static size_t
fd_write_write_method(FDWrite *self, const void *buf, size_t buflen)
{
  gint rc;
  
  do
    {
      if (self->timeout)
        alarm_set(self->timeout);
      rc = write(self->fd, buf, buflen);
      if (self->timeout > 0 && rc == -1 && errno == EINTR && alarm_has_fired())
        {
          msg_notice("Nonblocking write has blocked, returning with an error",
                     evt_tag_int("fd", self->fd),
                     evt_tag_int("timeout", self->timeout),
                     NULL);
          alarm_cancel();
          break;
        }
      if (self->timeout)
        alarm_cancel();
      if (self->fsync)
        fsync(self->fd);
    }
  while (rc == -1 && errno == EINTR);
  return rc;
}

FDWrite *
fd_write_new(gint fd)
{
  FDWrite *self = g_new0(FDWrite, 1);
  
  self->fd = fd;
  self->write = fd_write_write_method;
  self->free_fn = fd_write_free_method;
  self->cond = G_IO_OUT;
  return self;
}

void
fd_write_free_method(FDWrite *self)
{
  msg_verbose("Closing log writer fd",
              evt_tag_int("fd", self->fd),
              NULL);
  close(self->fd);

}

void
fd_write_free(FDWrite *self)
{
  self->free_fn(self);
  g_free(self);
}
