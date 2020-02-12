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
#include "poll-fd-events.h"

#include <iv.h>

typedef struct _PollFdEvents
{
  PollEvents super;
  struct iv_fd fd_watch;
} PollFdEvents;

#define IV_FD_CALLBACK(x) ((void (*)(void *)) (x))

static void
poll_fd_events_start_watches(PollEvents *s)
{
  PollFdEvents *self = (PollFdEvents *) s;

  iv_fd_register(&self->fd_watch);
}

static void
poll_fd_events_stop_watches(PollEvents *s)
{
  PollFdEvents *self = (PollFdEvents *) s;

  if (iv_fd_registered(&self->fd_watch))
    iv_fd_unregister(&self->fd_watch);
}

static void
poll_fd_events_suspend_watches(PollEvents *s)
{
  PollFdEvents *self = (PollFdEvents *) s;

  iv_fd_set_handler_in(&self->fd_watch, NULL);
  iv_fd_set_handler_out(&self->fd_watch, NULL);
  iv_fd_set_handler_err(&self->fd_watch, NULL);
}


static void
poll_fd_events_update_watches(PollEvents *s, GIOCondition cond)
{
  PollFdEvents *self = (PollFdEvents *) s;

  poll_events_suspend_watches(s);

  if (cond & G_IO_IN)
    iv_fd_set_handler_in(&self->fd_watch, IV_FD_CALLBACK(poll_events_invoke_callback));

  if (cond & G_IO_OUT)
    iv_fd_set_handler_out(&self->fd_watch, IV_FD_CALLBACK(poll_events_invoke_callback));

  if (cond & (G_IO_IN + G_IO_OUT))
    iv_fd_set_handler_err(&self->fd_watch, IV_FD_CALLBACK(poll_events_invoke_callback));
}

PollEvents *
poll_fd_events_new(gint fd)
{
  PollFdEvents *self = g_new0(PollFdEvents, 1);

  g_assert(fd >= 0);

  self->super.start_watches = poll_fd_events_start_watches;
  self->super.stop_watches = poll_fd_events_stop_watches;
  self->super.update_watches = poll_fd_events_update_watches;
  self->super.suspend_watches = poll_fd_events_suspend_watches;

  IV_FD_INIT(&self->fd_watch);
  self->fd_watch.fd = fd;
  self->fd_watch.cookie = self;

  return &self->super;
}
