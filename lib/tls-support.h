/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#ifndef TLS_SUPPORT_H_INCLUDED
#define TLS_SUPPORT_H_INCLUDED

/* this header is about Thread Local Storage and not about Transport Layer Security */

/* Some platforms and/or build-specific constellations can make
 * finding TLS variables in a debugger extremely hard, especially
 * in case of core files without a running process. To make it
 * easier, all implementations contain a cookie. To localize the
 * thread local storage in the memory, look for the string
 * "TLSBEG*\x00" and "TLSEND*\x00" where * can be one of 'P', 'W'
 * or 'T'. Between these two 8 character long strings, is the
 * struct that holds thread local variables. P, W and T means
 * pthread_*, Windows and __thread implementation respectively.
 * The exact format of the struct is as follows:
 *
 *     char beginning_cookie[8]             -- TLSBEG*\x00
 *     char *source                         -- Pointer to the name of the source file
 *     struct { user defined variables };
 *     char end_cookie[8]                   -- TLSEND*\x00
 *
 * Note that in case of POSIX threads, the cookies will only be
 * put in place when the first TLS variable is used by the program.
 */

#include "syslog-ng.h"

/* We don't have thread-local-storage support on Win32 and the code that
 * compiles on Win32 is not threaded at all.  If that changes, this code
 * certainly needs to change too. */

#if !HAVE_THREAD_KEYWORD

#include <stdlib.h>
#include <pthread.h>
#include <string.h>


#define TLS_BLOCK_START                                                     \
  static pthread_key_t __tls_key;                                           \
  static void __attribute__((constructor)) __tls_init_key(void)             \
  {                                                                         \
    pthread_key_create(&__tls_key, free);                                   \
  }                                                                         \
                                                                            \
  struct __tls_variables                                                    \
  {                                                                         \
    gchar beginning_cookie[8];                                              \
    gchar *source;                                                          \
    struct

#define TLS_BLOCK_END                                                           \
    variables;                                                                  \
    gchar end_cookie[8];                                                        \
  };                                                                            \
                                                                                \
  static struct __tls_variables *                                               \
  __tls_init_thread(pthread_key_t key, const gchar *source)                     \
  {                                                                             \
    struct __tls_variables *ptr;                                                \
                                                                                \
    ptr = calloc(1, sizeof(struct __tls_variables));                            \
    if (!ptr)                                                                   \
      abort();                                                                  \
                                                                                \
    strncpy(ptr->beginning_cookie, "TLSBEGP", sizeof(ptr->beginning_cookie));   \
    ptr->source = source;                                                       \
    strncpy(ptr->end_cookie, "TLSENDP", sizeof(ptr->end_cookie));               \
                                                                                \
    pthread_setspecific(key, ptr);                                              \
    return ptr;                                                                 \
  }                                                                             \
                                                                                \
  static inline struct __tls_variables *                                        \
  __tls_deref_helper(pthread_key_t key, const gchar *source)                    \
  {                                                                             \
    struct __tls_variables *ptr;                                                \
                                                                                \
    ptr = pthread_getspecific(key);                                             \
    if (!ptr)                                                                   \
      ptr = __tls_init_thread(key, source);                                     \
                                                                                \
    return ptr;                                                                 \
  }

#define __tls_deref(var)   (*({ struct __tls_variables *__ptr = __tls_deref_helper(__tls_key, __BASE_FILE__); &__ptr->variables.var; }))

#define __thread #

#else  /* HAVE_TLS */

#define TLS_BLOCK_START                                                     \
  struct __tls_variables                                                    \
  {                                                                         \
    gchar beginning_cookie[8];                                              \
    gchar *source;                                                          \
    struct

#define TLS_BLOCK_END                                                       \
    variables;                                                              \
    gchar end_cookie[8];                                                    \
  };                                                                        \
  static __thread struct __tls_variables __tls = {                          \
    .beginning_cookie   = { 'T', 'L', 'S', 'B', 'E', 'G', 'T', '\x00' },    \
    .source             = __BASE_FILE__,                                    \
    .end_cookie         = { 'T', 'L', 'S', 'E', 'N', 'D', 'T', '\x00' },    \
  }

#define __tls_deref(var)   (__tls.variables.var)

#endif

#endif
