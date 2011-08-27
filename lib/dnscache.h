/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
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


#ifndef DNSCACHE_H_INCLUDED
#define DNSCACHE_H_INCLUDED

#include "syslog-ng.h"

gboolean dns_cache_lookup(gint family, void *addr, const gchar **hostname, gboolean *positive);
void dns_cache_store(gboolean persistent, gint family, void *addr, const gchar *hostname, gboolean positive);

void dns_cache_set_params(gint cache_size, gint expire, gint expire_failed, const gchar *hosts);
void dns_cache_init(void);
void dns_cache_destroy(void);

#endif
