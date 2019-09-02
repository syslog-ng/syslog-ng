/*
 * Copyright (c) 2002-2013 Balabit
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
#include "cache.h"

struct _Cache
{
  GHashTable *hash_table;
  CacheResolver *resolver;
};

void *
cache_lookup(Cache *self, const gchar *key)
{
  gpointer result = g_hash_table_lookup(self->hash_table, key);

  if (!result)
    {
      result = cache_resolver_resolve_elem(self->resolver, key);
      if (result)
        {
          g_hash_table_insert(self->hash_table, g_strdup(key), result);
        }
    }
  return result;
}

void
cache_clear(Cache *self)
{
  g_hash_table_unref(self->hash_table);
  self->hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, self->resolver->free_elem);
}

Cache *
cache_new(CacheResolver *resolver)
{
  Cache *self = g_new0(Cache, 1);

  self->resolver = resolver;
  self->hash_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, resolver->free_elem);
  return self;
}

void
cache_free(Cache *self)
{
  cache_resolver_free(self->resolver);
  g_hash_table_unref(self->hash_table);
  g_free(self);
}
