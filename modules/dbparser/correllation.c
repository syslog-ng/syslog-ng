/*
 * Copyright (c) 2002-2013, 2015 BalaBit
 * Copyright (c) 1998-2013, 2015 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */
#include "correllation.h"
#include "correllation-key.h"

void
correllation_state_init_instance(CorrellationState *self, GDestroyNotify state_entry_free)
{
  self->state_entry_free = state_entry_free;
  self->state = g_hash_table_new_full(correllation_key_hash, correllation_key_equal, NULL, state_entry_free);
}

void
correllation_state_deinit_instance(CorrellationState *self)
{
  if (self->state)
    g_hash_table_destroy(self->state);
}

CorrellationState *
correllation_state_new(GDestroyNotify state_entry_free)
{
  CorrellationState *self = g_new0(CorrellationState, 1);

  correllation_state_init_instance(self, state_entry_free);
  return self;
}

void
correllation_state_free(CorrellationState *self)
{
  correllation_state_deinit_instance(self);
  g_free(self);
}
