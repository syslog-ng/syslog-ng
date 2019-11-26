/*
 * Copyright (c) 2019 Balabit
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

#include "serial-list.h"

typedef struct
{
  Offset prev;
  Offset next;
  gsize data_len;
  guchar data[];
} Node;

static Node *
get_node_at_offset(SerialList *self, Offset offset)
{
  g_assert((offset & 3) == 0);

  return (Node *)&self->base[offset];
}

static void
initialize_head(SerialList *self)
{
  Node *head = get_node_at_offset(self, 0);
  head->prev = 0;
  head->next = 0;
  head->data_len = 0;
}

SerialList *
serial_list_new(guchar *base, gsize size)
{
  g_assert(size > sizeof(Node));

  SerialList *self = g_new0(SerialList, 1);
  self->base = base;
  self->max_size = size;

  initialize_head(self);

  return self;
}

void
serial_list_free(SerialList *self)
{
  g_free(self);
}
