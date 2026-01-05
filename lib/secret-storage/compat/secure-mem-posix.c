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
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#include "secure-mem-posix.h"

#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

#ifndef _SC_PAGE_SIZE
#define _SC_PAGE_SIZE _SC_PAGESIZE
#endif

gsize
secure_mem_get_page_size(void)
{
  long ps = sysconf(_SC_PAGE_SIZE);
  if (ps <= 0) ps = 4096;
  return (gsize) ps;
}

gpointer
secure_mem_mmap_anon_rw(gsize len)
{
  void *area = mmap(NULL, len, PROT_READ | PROT_WRITE,
                    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (area == MAP_FAILED)
    return NULL;
  return area;
}

gboolean
secure_mem_exclude_from_core_dump(gpointer area, gsize len)
{
#ifdef MADV_DONTDUMP
  if (madvise(area, len, MADV_DONTDUMP) < 0)
    {
      if (errno == EINVAL)
        return TRUE;
      return FALSE;
    }
#endif
  return TRUE;
}

int
secure_mem_mlock(gpointer area, gsize len)
{
  return mlock(area, len);
}

int
secure_mem_munlock(gpointer area, gsize len)
{
  return munlock(area, len);
}

int
secure_mem_munmap(gpointer area, gsize len)
{
  return munmap(area, len);
}

void
secure_mem_zero(volatile void *p, gsize len)
{
  if (!p || !len) return;
  volatile unsigned char *q = (volatile unsigned char *) p;
  while (len--) *q++ = 0;
}

#endif