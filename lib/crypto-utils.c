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

/*
 * Look up a digest. MD4 and MD5 are only used for non-security purposes
 * (identifiers, user requested checksums), so on OpenSSL 3 they are fetched
 * with the "-fips" property query, which lets them come from a non-FIPS
 * provider even if the default properties request fips=yes. Approved
 * digests keep the default property query, so they still come from the
 * FIPS provider. Returns NULL if unavailable. Release with
 * crypto_free_digest().
 */
const EVP_MD *
crypto_fetch_digest(const gchar *name)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  gboolean non_security = !g_ascii_strcasecmp(name, "md5") || !g_ascii_strcasecmp(name, "md4");
  return EVP_MD_fetch(NULL, name, non_security ? "-fips" : NULL);
#else
  return EVP_get_digestbyname(name);
#endif
}

void
crypto_free_digest(const EVP_MD *md)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_MD_free((EVP_MD *) md);
#endif
}

/* Returns the length of the digest written to hash, or 0 on error. */
guint
compose_hash(const EVP_MD *md, GString *const *argv, gint argc, guchar *hash)
{
  DECLARE_EVP_MD_CTX(mdctx);
  EVP_MD_CTX_init(mdctx);
  guint md_len = 0;
  gboolean ok = EVP_DigestInit_ex(mdctx, md, NULL);

  for (gint i = 0; ok && i < argc; i++)
    ok = EVP_DigestUpdate(mdctx, argv[i]->str, argv[i]->len);

  if (ok)
    ok = EVP_DigestFinal_ex(mdctx, hash, &md_len);
  EVP_MD_CTX_cleanup(mdctx);
  EVP_MD_CTX_destroy(mdctx);

  return ok ? md_len : 0;
}
