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
#if defined (_WIN32)

#include "secure-mem-windows.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <errno.h>

#ifndef SecureZeroMemory
#define SecureZeroMemory RtlSecureZeroMemory
#endif

gsize
secure_mem_get_page_size(void)
{
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return (gsize) si.dwPageSize;
}

gpointer
secure_mem_mmap_anon_rw(gsize len)
{
  if (!len) return NULL;
  void *p = VirtualAlloc(NULL, (SIZE_T)len, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  return p ? p : NULL;
}

gboolean
secure_mem_exclude_from_core_dump(gpointer area, gsize len)
{
  (void)area;
  (void)len;
  return TRUE;
}

int
secure_mem_mlock(gpointer area, gsize len)
{
  if (!area || !len) return 0;
  return VirtualLock(area, (SIZE_T)len) ? 0 : -1;
}

int
secure_mem_munlock(gpointer area, gsize len)
{
  if (!area || !len) return 0;
  return VirtualUnlock(area, (SIZE_T)len) ? 0 : -1;
}

int
secure_mem_munmap(gpointer area, gsize len)
{
  (void)len;
  if (!area) return 0;
  return VirtualFree(area, 0, MEM_RELEASE) ? 0 : -1;
}

void
secure_mem_zero(volatile void *p, gsize len)
{
  if (!p || !len) return;
  SecureZeroMemory((PVOID)p, (SIZE_T)len);
}

#endif