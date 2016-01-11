/*
 * Copyright (c) 2002-2011 Balabit
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

#include <syslog-ng-config.h>

#ifndef SYSLOG_NG_HAVE_THREAD_KEYWORD

#include <stdlib.h>
#include <pthread.h>

static struct __tls_variables *
__tls_init_thread(pthread_key_t key, size_t size)
{
  struct __tls_variables *ptr;

  ptr = calloc(1, size);
  if (!ptr)
    abort();

  pthread_setspecific(key, ptr);
  return ptr;
}

static inline struct __tls_variables *__tls_deref_helper(pthread_key_t key, size_t size)
{
  struct __tls_variables *ptr;

  ptr = pthread_getspecific(key);
  if (!ptr)
    ptr = __tls_init_thread(key, size);
  return ptr;
}

#define TLS_BLOCK_START                                         \
  static pthread_key_t __tls_key;                               \
  static void __attribute__((constructor)) __tls_init_key(void) \
  {                                                             \
    pthread_key_create(&__tls_key, free);                       \
  }                                                             \
                                                                \
  struct __tls_variables

#define TLS_BLOCK_END

#define __tls_deref(var)   (*({ struct __tls_variables *__ptr = __tls_deref_helper(__tls_key, sizeof(struct __tls_variables)); &__ptr->var; }))

#define __thread #

#else  /* SYSLOG_NG_HAVE_TLS */

#define TLS_BLOCK_START                         \
  struct __tls_variables

#define TLS_BLOCK_END                           \
  ;                                             \
  static __thread struct __tls_variables __tls


#define __tls_deref(var)   (__tls.var)

#endif

#endif
