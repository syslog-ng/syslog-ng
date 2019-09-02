/*
 * Copyright (c) 2019 Balázs Scheidler <bazsi77@gmail.com>
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
#include "timeutils/zonecache.h"
#include "timeutils/zoneinfo.h"
#include "timeutils/zonedb.h"
#include "tls-support.h"
#include "../cache.h"

typedef struct _TimeZoneResolver
{
  CacheResolver super;
} TimeZoneResolver;

static gpointer
time_zone_resolver_resolve(CacheResolver *s, const gchar *tz)
{
  if (is_time_zone_valid(tz))
    return time_zone_info_new(tz);
  return NULL;
}

static CacheResolver *
time_zone_resolver_new(void)
{
  TimeZoneResolver *self = g_new0(TimeZoneResolver, 1);

  self->super.resolve_elem = time_zone_resolver_resolve;
  self->super.free_elem = (GDestroyNotify) time_zone_info_free;

  return &self->super;
}

Cache *
time_zone_cache_new(void)
{
  return cache_new(time_zone_resolver_new());
}
