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

#ifndef _WIN32

#include <sys/mman.h>

#else /* POSIX */

#include <stddef.h>
#include <stdint.h>

/* flags/prot (minimal) */
#ifndef PROT_READ
#define PROT_READ   0x1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE  0x2
#endif
#ifndef MAP_SHARED
#define MAP_SHARED  0x01
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 0x02
#endif
#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif
#ifndef MS_SYNC
#define MS_SYNC 0x4
#endif

/* POSIX-like signatures */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, long long offset);
int   munmap(void *addr, size_t length);
int   msync(void *addr, size_t length, int flags);

#endif