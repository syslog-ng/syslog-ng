/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef DNSCACHE_H_INCLUDED
#define DNSCACHE_H_INCLUDED

#include "syslog-ng.h"

gboolean dns_cache_lookup(gint family, void *addr, const gchar **hostname);
void dns_cache_store(gboolean persistent, gint family, void *addr, const gchar *hostname);

void dns_cache_set_params(gint cache_size, gint expire, gint expire_failed, const gchar *hosts);
void dns_cache_init(void);
void dns_cache_destroy(void);

#endif
