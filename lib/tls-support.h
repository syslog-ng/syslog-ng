#ifndef TLS_SUPPORT_H_INCLUDED
#define TLS_SUPPORT_H_INCLUDED

/* this header is about Thread Local Storage and not about Transport Layer Security */

#include <config.h>

#ifndef HAVE_THREAD_KEYWORD

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

#else  /* HAVE_TLS */

#define TLS_BLOCK_START                         \
  struct __tls_variables

#define TLS_BLOCK_END                           \
  ;                                             \
  static __thread struct __tls_variables __tls


#define __tls_deref(var)   (__tls.var)

#endif

#endif
