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
#include "string.h"

struct _SerialHash
{
  GHashTable *index;
  SerialList *storage;
};

static gsize
align(gsize size)
{
  if (size % sizeof(guint64) == 0)
    return size;

  return size + (sizeof(guint64) - size % sizeof(guint64));
}

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

/* Cannot be expressed with struct, because it has two embedded variable length data: key and value
   The structure is the following:

|gsize: key-block| key (and possible alignment)|gsize: value-len| value   (and possible alignment|
                 \-------length: key-block-----|                \-----length: payload-len--------|
*/
void
payload_new(gchar *key, guchar *value, gsize value_len, guchar **payload, gsize *payload_len)
{
  gsize key_block_length = align(strlen(key) + 1);
  gsize total_length = sizeof(gsize) + key_block_length + sizeof(gsize) + value_len;
  guchar *data = g_new(guchar, total_length);
  *payload = data;
  *payload_len = total_length;

  *((gsize *)data) = key_block_length;
  strcpy((gchar *)(data + sizeof(gsize)), key);
  *((gsize *)(data + sizeof(gsize) + key_block_length)) = value_len;
  memcpy(data + sizeof(gsize) + key_block_length + sizeof(gsize), value, value_len);
}

gchar *
payload_get_key(guchar *payload)
{
  return (gchar *)(payload + sizeof(gsize));
}

void
payload_get_value(guchar *payload, guchar **value, gsize *value_len)
{
  gsize key_block_length = *((gsize *)payload);
  *value_len = *((gsize *)(payload + sizeof(gsize) + key_block_length));
  *value = (guchar *)(payload + sizeof(gsize) +  key_block_length + sizeof(gsize));
}

void
payload_free(guchar *payload)
{
  g_free(payload);
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
