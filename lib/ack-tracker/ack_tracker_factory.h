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
#include "atomic.h"
#include "batched_ack_tracker.h"

struct _AckTrackerFactory
{
  GAtomicCounter ref_cnt;
  AckTrackerType type;
  AckTracker *(*create)(AckTrackerFactory *self, LogSource *source);
  void (*free_fn)(AckTrackerFactory *self);
};

static inline AckTracker *
ack_tracker_factory_create(AckTrackerFactory *self, LogSource *source)
{
  g_assert(self && self->create);
  return self->create(self, source);
}

static inline AckTrackerType
ack_tracker_factory_get_type(AckTrackerFactory *self)
{
  g_assert(self);
  return self->type;
}

void ack_tracker_factory_init_instance(AckTrackerFactory *self);
AckTrackerFactory *ack_tracker_factory_ref(AckTrackerFactory *self);
void ack_tracker_factory_unref(AckTrackerFactory *self);

AckTrackerFactory *instant_ack_tracker_factory_new(void);
AckTrackerFactory *instant_ack_tracker_bookmarkless_factory_new(void);
AckTrackerFactory *consecutive_ack_tracker_factory_new(void);
AckTrackerFactory *batched_ack_tracker_factory_new(guint timeout, guint batch_size,
                                                   BatchedAckTrackerOnBatchAcked cb,
                                                   gpointer user_data);

#endif
