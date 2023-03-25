/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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
#include "cfg-persist.h"
#include "messages.h"

typedef struct _PersistConfigEntry
{
  gpointer value;
  GDestroyNotify destroy;
} PersistConfigEntry;

static void
persist_config_entry_free(PersistConfigEntry *self)
{
  if (self->destroy)
    {
      self->destroy(self->value);
    }
  g_free(self);
}

void
persist_config_add(PersistConfig *self, const gchar *name, gpointer value, GDestroyNotify destroy)
{
  PersistConfigEntry *p;

  if (g_hash_table_lookup(self->keys, name))
    {
      msg_error("Internal error, duplicate configuration elements refer to the same persistent config",
                evt_tag_str("name", name));
      if (destroy)
        destroy(value);
      return;
    }

  p = g_new0(PersistConfigEntry, 1);

  p->value = value;
  p->destroy = destroy;
  g_hash_table_insert(self->keys, g_strdup(name), p);
  return;
}

gpointer
persist_config_fetch(PersistConfig *self, const gchar *name)
{
  gpointer res = NULL;
  gchar *orig_key;
  PersistConfigEntry *p;
  gpointer tmp1, tmp2;

  if (g_hash_table_lookup_extended(self->keys, name, &tmp1, &tmp2))
    {
      orig_key = (gchar *) tmp1;
      p = (PersistConfigEntry *) tmp2;

      res = p->value;

      g_hash_table_steal(self->keys, name);
      g_free(orig_key);
      g_free(p);
    }
  return res;
}

PersistConfig *
persist_config_new(void)
{
  PersistConfig *self = g_new0(PersistConfig, 1);

  self->keys = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify) g_free,
                                     (GDestroyNotify) persist_config_entry_free);
  return self;
}

void
persist_config_free(PersistConfig *self)
{
  g_hash_table_destroy(self->keys);
  g_free(self);
}
