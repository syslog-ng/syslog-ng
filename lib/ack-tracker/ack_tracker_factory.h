/*
 * Copyright (c) 2020 One Identity
 * Copyright (c) 2020 Laszlo Budai
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

#ifndef ACK_TRACKER_FACTORY_H_INCLUDED
#define ACK_TRACKER_FACTORY_H_INCLUDED

#include "syslog-ng.h"
#include "logsource.h"
#include "ack_tracker_types.h"

typedef struct _AckTrackerFactory AckTrackerFactory;

struct _AckTrackerFactory
{
  AckTracker *(*create)(AckTrackerFactory *self, LogSource *source);
  void (*free_fn)(AckTrackerFactory *self);
};

static inline AckTracker *
ack_tracker_factory_create(AckTrackerFactory *self, LogSource *source)
{
  g_assert(self && self->create);
  return self->create(self, source);
}

static inline void
ack_tracker_factory_free(AckTrackerFactory *self)
{
  if (self && self->free_fn)
    self->free_fn(self);
}

AckTrackerFactory *ack_tracker_factory_new(AckTrackerType type);

#endif
