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
#ifndef POLL_EVENTS_H_INCLUDED
#define POLL_EVENTS_H_INCLUDED

#include "syslog-ng.h"

typedef enum
{
  FM_AUTO,
  FM_POLL,
  FM_SYSTEM_POLL,
  FM_INOTIFY,
  FM_UNKNOWN
} FollowMethod;

typedef struct _PollEvents PollEvents;
typedef void (*PollCallback)(gpointer user_data);
typedef gboolean (*PollChecker)(PollEvents *self, gpointer user_data);

/* TODO: Poll is just a legacy word here, rename to sg. more appropriate (e.g. FollowEvents) */
struct _PollEvents
{
  FollowMethod type;
  PollCallback callback;
  gpointer callback_data;
  gpointer checker_data;

  void (*start_watches)(PollEvents *self);
  void (*stop_watches)(PollEvents *self);
  PollChecker check_watches;
  void (*update_watches)(PollEvents *self, GIOCondition cond);
  void (*suspend_watches)(PollEvents *self);
  gint (*get_fd)(PollEvents *self);
  void (*free_fn)(PollEvents *self);
};

static inline gint
poll_events_get_fd(PollEvents *self)
{
  if (self->get_fd)
    return self->get_fd(self);
  return -1;
}

static inline gboolean
poll_events_system_polled(PollEvents *self)
{
  return self->type == FM_SYSTEM_POLL;
}

static inline gboolean
poll_events_system_notified(PollEvents *self)
{
  return self->type == FM_INOTIFY;
}

static inline void
poll_events_start_watches(PollEvents *self)
{
  if (self->start_watches)
    self->start_watches(self);
}

static inline void
poll_events_stop_watches(PollEvents *self)
{
  if (self->stop_watches)
    self->stop_watches(self);
}

static inline gboolean
poll_events_check_watches(PollEvents *self)
{
  if (self->check_watches)
    return self->check_watches(self, self->checker_data);
  return TRUE;
}

static inline void
poll_events_update_watches(PollEvents *self, GIOCondition cond)
{
  self->update_watches(self, cond);
}

static inline void
poll_events_suspend_watches(PollEvents *self)
{
  if (self->suspend_watches)
    return self->suspend_watches(self);
  poll_events_stop_watches(self);
}

void poll_events_invoke_callback(PollEvents *self);
void poll_events_set_callback(PollEvents *self, PollCallback callback, gpointer user_data);
void poll_events_set_checker(PollEvents *self, PollChecker check_watches, gpointer user_data);
void poll_events_init(PollEvents *self);
void poll_events_free(PollEvents *self);

#endif
