/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
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

/* This file becomes part of libsyslog-ng-crypto.so, the shared object
 * that contains all crypto related stuff to be used by plugins. This
 * includes the TLS wrappers, random number initialization, and so on.
 */

#include "crypto.h"
#include "apphook.h"
#include "thread-utils.h"

#if ENABLE_SSL

#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <stdio.h>

static gint ssl_lock_count;
static GStaticMutex *ssl_locks;
static gboolean randfile_loaded;

static void
ssl_locking_callback(int mode, int type, char *file, int line)
{
  if (mode & CRYPTO_LOCK)
    {
      g_static_mutex_lock(&ssl_locks[type]);
    }
  else
    {
      g_static_mutex_unlock(&ssl_locks[type]);
    }
}

static unsigned long
ssl_thread_id(void)
{
  return (unsigned long) get_thread_id();
}

static void
crypto_init_threading(void)
{
  gint i;

  ssl_lock_count = CRYPTO_num_locks();
  ssl_locks = g_new(GStaticMutex, ssl_lock_count);
  for (i = 0; i < ssl_lock_count; i++)
    {
      g_static_mutex_init(&ssl_locks[i]);
    }
  CRYPTO_set_id_callback((unsigned long (*)()) ssl_thread_id);
  CRYPTO_set_locking_callback((void (*)()) ssl_locking_callback);
}

static void
crypto_deinit_threading(void)
{
  gint i;

  for (i = 0; i < ssl_lock_count; i++)
    {
      g_static_mutex_free(&ssl_locks[i]);
    }
  g_free(ssl_locks);
}

void
crypto_deinit(void)
{
  char rnd_file[256];

  if (randfile_loaded)
    {
      RAND_file_name(rnd_file, sizeof(rnd_file));
      if (rnd_file[0])
        RAND_write_file(rnd_file);
    }
  crypto_deinit_threading();
}

static void
crypto_init(void)
{
  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();
  crypto_init_threading();

  if (RAND_status() < 0 || getenv("RANDFILE"))
    {
      char rnd_file[256];

      RAND_file_name(rnd_file, sizeof(rnd_file));
      if (rnd_file[0])
        {
          RAND_load_file(rnd_file, -1);
          randfile_loaded = TRUE;
        }

      if (RAND_status() < 0)
        fprintf(stderr, "WARNING: a trusted random number source is not available, crypto operations will probably fail. Please set the RANDFILE environment variable.");
    }
}

static void __attribute__((constructor))
crypto_load(void)
{
  crypto_init();
}

static void __attribute__((destructor))
crypto_unload(void)
{
  crypto_deinit();
}

/* the crypto options (seed) are handled in main.c */

#endif
