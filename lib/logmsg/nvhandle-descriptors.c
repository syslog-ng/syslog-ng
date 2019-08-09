/*
 * Copyright (c) 2018 Balabit
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

#include "string.h"

#include "nvhandle-descriptors.h"
#include "syslog-ng.h"

NVHandleDescArray *
nvhandle_desc_array_new(guint reserved_size)
{
  NVHandleDescArray *self = g_new0(NVHandleDescArray, 1);
  self->data = g_new0(NVHandleDesc, reserved_size);
  self->allocated_len = reserved_size;
  self->old_buffers = g_ptr_array_new_with_free_func(g_free);
  return self;
}

void
nvhandle_desc_array_free(NVHandleDescArray *self)
{
  for (gint i=0; i < self->len; ++i)
    {
      NVHandleDesc *handle = (NVHandleDesc *)&self->data[i];
      nvhandle_desc_free(handle);
    }
  g_free(self->data);
  g_ptr_array_free(self->old_buffers, TRUE);
  g_free(self);
}

static void
nvhandle_desc_array_expand(NVHandleDescArray *self)
{
  guint new_alloc = self->allocated_len * 2;
  NVHandleDesc *new_data = g_new(NVHandleDesc, new_alloc);
  g_assert(new_data);

  memcpy(new_data, self->data, self->len * sizeof(NVHandleDesc));

  g_ptr_array_add(self->old_buffers, self->data);
  self->data = new_data;
  self->allocated_len = new_alloc;
}

void
nvhandle_desc_array_append(NVHandleDescArray *self, NVHandleDesc *desc)
{
  if (self->len == self->allocated_len)
    nvhandle_desc_array_expand(self);

  self->data[self->len] = *desc;
  self->len++;
}

void
nvhandle_desc_free(NVHandleDesc *self)
{
  g_free(self->name);
}
