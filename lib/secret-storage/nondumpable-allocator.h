/*
 * Copyright (c) 2018 Balabit
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

#ifndef NONDUMPABLE_ALLOCATOR_H_INCLUDED
#define NONDUMPABLE_ALLOCATOR_H_INCLUDED

#include "lib/compat/glib.h"

#define PUBLIC __attribute__ ((visibility ("default")))
#define INTERNAL __attribute__ ((visibility ("hidden")))

gpointer nondumpable_buffer_alloc(gsize len) PUBLIC;
void nondumpable_buffer_free(gpointer buffer) PUBLIC;
gpointer nondumpable_buffer_realloc(gpointer buffer, gsize len) PUBLIC;
gpointer nondumpable_memcpy(gpointer dest, gpointer src, gsize len) PUBLIC;

#endif
