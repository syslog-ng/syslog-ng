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

#ifdef _WIN32

#include "mmap.h"
#include <windows.h>
#include <io.h>       /* _get_osfhandle */
#include <stdlib.h>

/* track mapping handles so we can CloseHandle() at munmap() */
typedef struct MapNode
{
  void *base;
  HANDLE hmap;
  struct MapNode *next;
} MapNode;

static MapNode *g_maps;

static void add_map(void *base, HANDLE hmap)
{
  MapNode *n = (MapNode *)malloc(sizeof(*n));
  n->base = base;
  n->hmap = hmap;
  n->next = g_maps;
  g_maps = n;
}

static HANDLE take_map(void *base)
{
  MapNode **pp=&g_maps;
  while (*pp)
    {
      if ((*pp)->base == base)
        {
          MapNode *n=*pp;
          *pp=n->next;
          HANDLE h=n->hmap;
          free(n);
          return h;
        }
      pp=&(*pp)->next;
    }
  return NULL;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, long long offset)
{
  DWORD protect = (prot & PROT_WRITE) ? PAGE_READWRITE : PAGE_READONLY;
  DWORD access  = (prot & PROT_WRITE) ? FILE_MAP_WRITE  : FILE_MAP_READ;

  HANDLE hfile = (HANDLE)_get_osfhandle(fd);
  if (hfile == INVALID_HANDLE_VALUE)
    return MAP_FAILED;

  LARGE_INTEGER maxsz;
  maxsz.QuadPart = offset + (long long)length;
  HANDLE hmap = CreateFileMappingW(hfile, NULL, protect, maxsz.HighPart, maxsz.LowPart, NULL);
  if (!hmap)
    return MAP_FAILED;

  LARGE_INTEGER off;
  off.QuadPart = offset;
  void *base = MapViewOfFile(hmap, access, off.HighPart, off.LowPart, length);
  if (!base)
    {
      CloseHandle(hmap);
      return MAP_FAILED;
    }

  add_map(base, hmap);
  return base;
}

int munmap(void *addr, size_t length)
{
  HANDLE hmap = take_map(addr);
  BOOL ok1 = UnmapViewOfFile(addr);
  if (hmap) CloseHandle(hmap);
  return ok1 ? 0 : -1;
}

int msync(void *addr, size_t length, int flags)
{
  (void)flags;
  /* Flush the view; flushing the file handle is usually unnecessary here */
  return FlushViewOfFile(addr, length) ? 0 : -1;
}

#endif