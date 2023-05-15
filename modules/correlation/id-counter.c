/*
 * Copyright (c) 2023 OneIdentity LLC.
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

#include "glib.h"

typedef struct _IdCounter
{
  guint id;
  guint ref_count;
} IdCounter;
IdCounter

*id_counter_new(void)
{
  IdCounter *self = g_new(IdCounter, 1);
  self->id = 0;
  self->ref_count = 1;
  return self;
}

guint
id_counter_get_next_id(IdCounter *self)
{
  return self->id++;
}

IdCounter
*id_counter_ref(IdCounter *self)
{
  if (self == NULL) return NULL;

  self->ref_count += 1;

  return self;
}

void
id_counter_unref(IdCounter *self)
{
  if (self == NULL) return;

  self->ref_count -= 1;

  if (self->ref_count == 0) g_free(self);
}
