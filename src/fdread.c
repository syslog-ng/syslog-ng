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

#include "fdread.h"
#include "messages.h"

#include <errno.h>

static size_t
fd_do_read(FDRead *self, void *buf, size_t buflen, GSockAddr **sa)
{
  gint rc;
  
  if ((self->flags & FR_RECV) == 0)
    {
      *sa = NULL;
      do
        {
          rc = read(self->fd, buf, buflen);
        }
      while (rc == -1 && errno == EINTR);
    }
  else 
    {
      struct sockaddr_storage sas;
      socklen_t salen = sizeof(sas);

      do
        {
          rc = recvfrom(self->fd, buf, buflen, 0, 
                        (struct sockaddr *) &sas, &salen);
        }
      while (rc == -1 && errno == EINTR);
      if (rc != -1 && salen)
        (*sa) = g_sockaddr_new((struct sockaddr *) &sas, salen);
    }
  return rc;
}

FDRead *
fd_read_new(gint fd, guint flags)
{
  FDRead *self = g_new0(FDRead, 1);
  
  self->fd = fd;
  self->cond = G_IO_IN;
  self->read = fd_do_read;
  self->flags = flags;
  return self;
}

void
fd_read_free(FDRead *self)
{
  if ((self->flags & FR_DONTCLOSE) == 0)
    {
      msg_verbose("Closing log reader fd", 
                  evt_tag_int(EVT_TAG_FD, self->fd), 
                  NULL);
      close(self->fd);
    }
  g_free(self);
}
