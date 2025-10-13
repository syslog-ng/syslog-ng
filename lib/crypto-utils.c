/*
 * Copyright (c) 2002-2014 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
 * Copyright (c) 2025 One Identity LLC.
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

#include "crypto-utils.h"
#include "compat/openssl_support.h"

guint
compose_hash(const EVP_MD *md, GString *const *argv, gint argc, guchar *hash)
{
  DECLARE_EVP_MD_CTX(mdctx);
  EVP_MD_CTX_init(mdctx);
  EVP_DigestInit_ex(mdctx, md, NULL);

  for (gint i = 0; i < argc; i++)
    EVP_DigestUpdate(mdctx, argv[i]->str, argv[i]->len);

  guint md_len;
  EVP_DigestFinal_ex(mdctx, hash, &md_len);
  EVP_MD_CTX_cleanup(mdctx);
  EVP_MD_CTX_destroy(mdctx);

  return md_len;
}
