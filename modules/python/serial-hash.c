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

#include "serial-hash.h"
#include "serial-list.h"

struct _SerialHash
{
  GHashTable *index;
  SerialList *storage;
};

SerialHash *
serial_hash_new(guchar *base, gsize max_size)
{
  SerialHash *self = g_new0(SerialHash, 1);
  self->index = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  self->storage = serial_list_new(base, max_size);
  return self;
}

void
serial_hash_free(SerialHash *self)
{
  if (!self)
    return;

  g_hash_table_destroy(self->index);
  self->index = NULL;
  serial_list_free(self->storage);
  self->storage = NULL;
}

static gboolean
_update(SerialHash *self, gchar *key, guchar *value, gsize value_len)
{
  return TRUE;
};

static gboolean
_insert(SerialHash *self, gchar *key, guchar *value, gsize value_len)
{
  return TRUE;
};

gboolean
serial_hash_insert(SerialHash *self, gchar *key, guchar *value, gsize value_len)
{
  if (g_hash_table_contains(self->index, key))
    return _update(self, key, value, value_len);
  else
    return _insert(self, key, value, value_len);
};
