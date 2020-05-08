/*
 * Copyright (c) 2011-2015 Balabit
 * Copyright (c) 2011-2015 Balázs Scheidler
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
#include "compat/openssl_support.h"

#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#include <stdio.h>

static gboolean randfile_loaded;

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
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ERR_free_strings();
  EVP_cleanup();
#endif
  openssl_crypto_deinit_threading();
}

void
crypto_init(void)
{
  openssl_init();
  openssl_crypto_init_threading();

  if (getenv("RANDFILE"))
    {
      char rnd_file[256];

      RAND_file_name(rnd_file, sizeof(rnd_file));
      if (rnd_file[0])
        {
          RAND_load_file(rnd_file, -1);
          randfile_loaded = TRUE;
          if (RAND_status() < 0)
            {
              fprintf(stderr,
                      "ERROR: a trusted random number source is not available, crypto operations will probably fail. This could be due to the lack of entropy in the RANDFILE or due to insufficient entropy provided by system sources.");
              g_assert_not_reached();
            }
        }
    }
}
