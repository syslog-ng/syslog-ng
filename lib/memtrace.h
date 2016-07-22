/*
 * Copyright (c) 2002-2010 Balabit
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

#ifndef ZORP_MEMTRACE_H_INCLUDED
#define ZORP_MEMTRACE_H_INCLUDED

void z_mem_trace_init(const gchar *memtrace_file);
void z_mem_trace_stats(void);
void z_mem_trace_dump(void);

#if SYSLOG_NG_ENABLE_MEMTRACE

#include <stdlib.h>

void *z_malloc(size_t size, gpointer backtrace[]);
void z_free(void *ptr, gpointer backtrace[]);
void *z_realloc(void *ptr, size_t size, gpointer backtrace[]);
void *z_calloc(size_t nmemb, size_t size, gpointer backtrace[]);

#endif

#endif
