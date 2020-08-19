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

#include "ack_tracker_factory.h"
#include "instant_ack_tracker.h"

typedef struct _InstantAckTrackerFactory
{
  AckTrackerFactory super;
} InstantAckTrackerFactory;

static AckTracker *
_instant_factory_create(AckTrackerFactory *s, LogSource *source)
{
  return instant_ack_tracker_new(source);
}

static void
_instant_factory_free(AckTrackerFactory *s)
{
  InstantAckTrackerFactory *self = (InstantAckTrackerFactory *)s;
  g_free(self);
}

static void
_init_instance(AckTrackerFactory *s)
{
  ack_tracker_factory_init_instance(s);

  s->create = _instant_factory_create;
  s->free_fn = _instant_factory_free;
  s->type = ACK_INSTANT;
}

AckTrackerFactory *
instant_ack_tracker_factory_new(void)
{
  InstantAckTrackerFactory *factory = g_new0(InstantAckTrackerFactory, 1);
  _init_instance(&factory->super);

  return &factory->super;
}
