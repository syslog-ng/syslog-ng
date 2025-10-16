/*
 * Copyright (c) 2018 Balabit
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

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "compat/secure-mem.h"

/* For compatibility with e.g. older macOS. */
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

#include "nondumpable-allocator.h"

#define ALLOCATION_HEADER_SIZE offsetof(Allocation, user_data)
#define BUFFER_TO_ALLOCATION(buffer) ((Allocation *) ((guint8 *) buffer - ALLOCATION_HEADER_SIZE))

static void
_silent(gchar *summary, gchar *reason)
{
}

NonDumpableLogger logger_debug_fn INTERNAL = _silent;
NonDumpableLogger logger_fatal_fn INTERNAL = _silent;

void
nondumpable_setlogger(NonDumpableLogger _debug, NonDumpableLogger _fatal)
{
  logger_debug_fn = _debug;
  logger_fatal_fn = _fatal;
}

#define logger_debug(summary, fmt, ...) \
{ \
 gchar *reason = g_strdup_printf(fmt, __VA_ARGS__); \
 logger_debug_fn(summary, reason); \
 g_free(reason); \
}

#define logger_fatal(summary, fmt, ...) \
{ \
 gchar *reason = g_strdup_printf(fmt, __VA_ARGS__); \
 logger_fatal_fn(summary, reason); \
 g_free(reason); \
}

typedef struct
{
  gsize alloc_size;
  gsize data_len;
  guint8 user_data[];
} Allocation;

static gpointer
_alloc_locked(gsize len)
{
  gpointer area = secure_mem_mmap_anon_rw(len);
  if (!area)
    {
      logger_fatal("secret storage: cannot mmap buffer",
                   "len: %"G_GSIZE_FORMAT", errno: %d\n", len, errno);
      return NULL;
    }

  if (!secure_mem_exclude_from_core_dump(area, len))
    goto err_unmap;

  if (secure_mem_mlock(area, len) < 0)
    {
      gchar *hint = (errno == ENOMEM) ? ". Maybe RLIMIT_MEMLOCK is too small?" : "";
      logger_fatal("secret storage: cannot lock buffer",
                   "len: %"G_GSIZE_FORMAT", errno: %d%s\n", len, errno, hint);
      goto err_unmap;
    }

  return area;
err_unmap:
  secure_mem_munmap(area, len);
  return NULL;
}

static gsize
round_to_nearest(gsize number, gsize base)
{
  return number + (base - (number % base));
}

gpointer
nondumpable_buffer_alloc(gsize len)
{
  gsize minimum_size = len + ALLOCATION_HEADER_SIZE;
  gsize pagesize = secure_mem_get_page_size();
  gsize alloc_size = round_to_nearest(minimum_size, pagesize);

  Allocation *buffer = _alloc_locked(alloc_size);
  if (!buffer)
    return NULL;

  buffer->alloc_size = alloc_size;
  buffer->data_len = len;
  return buffer->user_data;
}

void
nondumpable_mem_zero(gpointer s, gsize len)
{
  secure_mem_zero(s, len);
}

void
nondumpable_buffer_free(gpointer buffer)
{
  Allocation *allocation = BUFFER_TO_ALLOCATION(buffer);
  nondumpable_mem_zero(allocation->user_data, allocation->data_len);
  secure_mem_munmap(allocation, allocation->alloc_size);
}

gpointer
nondumpable_buffer_realloc(gpointer buffer, gsize len)
{
  Allocation *allocation = BUFFER_TO_ALLOCATION(buffer);
  if (allocation->alloc_size >= len + ALLOCATION_HEADER_SIZE)
    {
      allocation->data_len = len;
      return allocation->user_data;
    }

  gpointer new_buffer = nondumpable_buffer_alloc(len);
  nondumpable_memcpy(new_buffer, allocation->user_data, allocation->data_len);
  nondumpable_buffer_free(buffer);
  return new_buffer;
}

/* glibc implementation of memcpy can use stack, exposing parts of the secrets */
gpointer
nondumpable_memcpy(gpointer dest, gpointer src, gsize len)
{
  gchar *_dest = dest;
  gchar *_src = src;
  for (gsize i = 0; i < len; i++)
    {
      _dest[i] = _src[i];
    }

  return dest;
}
