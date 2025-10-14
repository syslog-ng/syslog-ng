/*
 * Copyright (c) 2025 One Identity
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

#pragma once
#include <glib.h>

//G_BEGIN_DECLS

gsize secure_mem_get_page_size(void);
gpointer secure_mem_mmap_anon_rw(gsize len);
gboolean secure_mem_exclude_from_core_dump(gpointer area, gsize len);
int secure_mem_mlock(gpointer area, gsize len);
int secure_mem_munlock(gpointer area, gsize len);
int secure_mem_munmap(gpointer area, gsize len);
void secure_mem_zero(volatile void *p, gsize len);

//G_END_DECLS