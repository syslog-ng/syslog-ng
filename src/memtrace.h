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

#ifndef ZORP_MEMTRACE_H_INCLUDED
#define ZORP_MEMTRACE_H_INCLUDED

void z_mem_trace_init(gchar *memtrace_file);
void z_mem_trace_stats(void);
void z_mem_trace_dump(void);

#if ENABLE_MEM_TRACE

#include <stdlib.h>

void *z_malloc(size_t size, gpointer backtrace[]);
void z_free(void *ptr, gpointer backtrace[]);
void *z_realloc(void *ptr, size_t size, gpointer backtrace[]);
void *z_calloc(size_t nmemb, size_t size, gpointer backtrace[]);

#endif

#endif
