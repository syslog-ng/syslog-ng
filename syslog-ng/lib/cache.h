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
#ifndef CACHE_H_INCLUDED
#define CACHE_H_INCLUDED

#include "syslog-ng.h"

typedef struct _CacheResolver CacheResolver;
typedef struct _Cache Cache;

struct _CacheResolver
{
  gpointer (*resolve_elem)(CacheResolver *self, const gchar *key);

  /* NOTE: free_elem lacks a self argument as we are using it as a
   * GDestroyNotify callback for hashtables.  Lacking a better solution
   * (e.g.  curried functions) this means that we can't pass self here.
   */
  void (*free_elem)(gpointer value);
  void (*free_fn)(CacheResolver *self);
};

static inline gpointer
cache_resolver_resolve_elem(CacheResolver *self, const gchar *key)
{
  if (self->resolve_elem)
    {
      return self->resolve_elem(self, key);
    }
  return NULL;
}

static inline void
cache_resolver_free(CacheResolver *self)
{
  if (self->free_fn)
    {
      self->free_fn(self);
    }
  g_free(self);
}

gpointer cache_lookup(Cache *self, const gchar *key);
void cache_clear(Cache *self);

Cache *cache_new(CacheResolver *resolver);
void cache_free(Cache *self);

#endif
