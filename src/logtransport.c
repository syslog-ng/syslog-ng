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

#include "logtransport.h"
#include "messages.h"
#include "alarms.h"

#include <errno.h>
#include <ctype.h>

void
log_transport_free_method(LogTransport *s)
{
  if ((s->flags & LTF_DONTCLOSE) == 0)
    {
      msg_verbose("Closing log transport fd",
                  evt_tag_int("fd", s->fd),
                  NULL);
      close(s->fd);
    }
}

void
log_transport_free(LogTransport *self)
{
  self->free_fn(self);
  g_free(self);
}

/* log transport that simply sends messages to an fd */
typedef struct _LogTransportPlain LogTransportPlain;

struct _LogTransportPlain
{
  LogTransport super;
};

static gssize
log_transport_plain_read_method(LogTransport *s, gpointer buf, gsize buflen, GSockAddr **sa)
{
  LogTransportPlain *self = (LogTransportPlain *) s;
  gint rc;
  
  if ((self->super.flags & LTF_RECV) == 0)
    {
      if (sa)
        *sa = NULL;

      do
        {
          if (self->super.timeout)
            alarm_set(self->super.timeout);
          rc = read(self->super.fd, buf, buflen);
          
          if (self->super.timeout > 0 && rc == -1 && errno == EINTR && alarm_has_fired())
            {
              msg_notice("Nonblocking read has blocked, returning with an error",
                         evt_tag_int("fd", self->super.fd),
                         evt_tag_int("timeout", self->super.timeout),
                         NULL);
              alarm_cancel();
              break;
            }
          if (self->super.timeout)
            alarm_cancel();
        }
      while (rc == -1 && errno == EINTR);
    }
  else 
    {
      union
      {
#if HAVE_STRUCT_SOCKADDR_STORAGE
        struct sockaddr_storage __sas;
#endif
        struct sockaddr __sa;
      } sas;
      
      socklen_t salen = sizeof(sas);

      do
        {
          rc = recvfrom(self->super.fd, buf, buflen, 0, 
                        (struct sockaddr *) &sas, &salen);
        }
      while (rc == -1 && errno == EINTR);
      if (rc != -1 && salen && sa)
        (*sa) = g_sockaddr_new((struct sockaddr *) &sas, salen);
    }
  return rc;
}

static gssize
log_transport_plain_write_method(LogTransport *s, const gpointer buf, gsize buflen)
{
  LogTransportPlain *self = (LogTransportPlain *) s;
  gint rc;
  
  do
    {
      if (self->super.timeout)
        alarm_set(self->super.timeout);
      if (self->super.flags & LTF_APPEND)
        lseek(self->super.fd, 0, SEEK_END);
      rc = write(self->super.fd, buf, buflen);
      if (self->super.timeout > 0 && rc == -1 && errno == EINTR && alarm_has_fired())
        {
          msg_notice("Nonblocking write has blocked, returning with an error",
                     evt_tag_int("fd", self->super.fd),
                     evt_tag_int("timeout", self->super.timeout),
                     NULL);
          alarm_cancel();
          break;
        }
      if (self->super.timeout)
        alarm_cancel();
      if (self->super.flags & LTF_FSYNC)
        fsync(self->super.fd);
    }
  while (rc == -1 && errno == EINTR);
  return rc;
}


LogTransport *
log_transport_plain_new(gint fd, guint flags)
{
  LogTransportPlain *self = g_new0(LogTransportPlain, 1);
  
  self->super.fd = fd;
  self->super.cond = 0;
  self->super.flags = flags;
  self->super.read = log_transport_plain_read_method;
  self->super.write = log_transport_plain_write_method;
  self->super.free_fn = log_transport_free_method;
  return &self->super;
}


