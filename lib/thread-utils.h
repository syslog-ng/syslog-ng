/*
 * Copyright (c) 2002-2013 Balabit
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
 */
#ifndef THREAD_UTILS_H_INCLUDED
#define THREAD_UTILS_H_INCLUDED

#include "syslog-ng.h"
#include <pthread.h>

#ifdef _WIN32
typedef DWORD ThreadId;
#else
typedef pthread_t ThreadId;
#endif

static inline ThreadId
get_thread_id(void)
{
#ifndef _WIN32
  return pthread_self();
#else
  return GetCurrentThreadId();
#endif
}

static inline int
threads_equal(ThreadId thread_a, ThreadId thread_b)
{
#ifndef _WIN32
  return pthread_equal(thread_a, thread_b);
#else
  return thread_a == thread_b;
#endif
}

static inline void
thread_cancel(ThreadId tid)
{
#ifndef _WIN32
  pthread_cancel(tid);
#else
  TerminateThread(tid, 0);
#endif
}

#endif
