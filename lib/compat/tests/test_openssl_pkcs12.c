/*
 * Copyright (c) 2026 One Identity LLC.
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

#include "compat/openssl_support.h"

#include <criterion/criterion.h>
#include <openssl/err.h>

static PKCS12 *
_load_bundle(const char *filename)
{
  gchar *path = g_build_filename(TOP_SRCDIR, "tests", "light", "shared_files", filename, NULL);
  BIO *bio = BIO_new_file(path, "rb");
  g_free(path);
  cr_assert_not_null(bio);
  PKCS12 *pkcs12 = d2i_PKCS12_bio(bio, NULL);
  BIO_free(bio);
  cr_assert_not_null(pkcs12);
  return pkcs12;
}

static void
_assert_parse(PKCS12 *pkcs12, const char *password, int expected_result)
{
  EVP_PKEY *private_key = NULL;
  X509 *cert = NULL;
  STACK_OF(X509) *ca_list = NULL;

  ERR_clear_error();
  int result = syslog_ng_pkcs12_parse(pkcs12, password, &private_key, &cert, &ca_list);
  cr_assert_eq(result, expected_result);
  if (result)
    {
      cr_assert_not_null(private_key);
      cr_assert_not_null(cert);
      cr_assert_eq(X509_check_private_key(cert, private_key), 1);
    }
  else
    {
      cr_assert_null(private_key);
      cr_assert_null(cert);
      cr_assert_neq(ERR_peek_error(), 0);
    }

  EVP_PKEY_free(private_key);
  X509_free(cert);
  sk_X509_pop_free(ca_list, X509_free);
}

Test(openssl_pkcs12, passwordless)
{
  PKCS12 *pkcs12 = _load_bundle("server.p12");
  _assert_parse(pkcs12, "", 1);
  _assert_parse(pkcs12, NULL, 1);
  PKCS12_free(pkcs12);
}

Test(openssl_pkcs12, protected_mac)
{
  PKCS12 *pkcs12 = _load_bundle("server-protected-asdfg.p12");
  _assert_parse(pkcs12, "asdfg", 1);
  PKCS12_free(pkcs12);
}

Test(openssl_pkcs12, protected_without_mac)
{
  PKCS12 *pkcs12 = _load_bundle("server-protected-nomac-asdfg.p12");
  _assert_parse(pkcs12, "asdfg", 1);
  PKCS12_free(pkcs12);
}

Test(openssl_pkcs12, wrong_password_with_mac_is_retryable)
{
  PKCS12 *pkcs12 = _load_bundle("server-protected-asdfg.p12");
  _assert_parse(pkcs12, "", 0);
  _assert_parse(pkcs12, "wrong-password", 0);
  _assert_parse(pkcs12, "asdfg", 1);
  PKCS12_free(pkcs12);
}

Test(openssl_pkcs12, wrong_password_without_mac_is_retryable)
{
  PKCS12 *pkcs12 = _load_bundle("server-protected-nomac-asdfg.p12");
  _assert_parse(pkcs12, "", 0);
  _assert_parse(pkcs12, "wrong-password", 0);
  _assert_parse(pkcs12, "asdfg", 1);
  PKCS12_free(pkcs12);
}

Test(openssl_pkcs12, original_without_mac_is_unchanged)
{
  PKCS12 *pkcs12 = _load_bundle("server-protected-nomac-asdfg.p12");
  unsigned char *before = NULL;
  unsigned char *after = NULL;
  int before_len = i2d_PKCS12(pkcs12, &before);
  cr_assert_gt(before_len, 0);

  _assert_parse(pkcs12, "wrong-password", 0);
  _assert_parse(pkcs12, "asdfg", 1);

  cr_assert_eq(PKCS12_mac_present(pkcs12), 0);
  int after_len = i2d_PKCS12(pkcs12, &after);
  cr_assert_eq(after_len, before_len);
  cr_assert_arr_eq(before, after, before_len);
  OPENSSL_free(before);
  OPENSSL_free(after);
  PKCS12_free(pkcs12);
}

Test(openssl_pkcs12, existing_mac_is_not_replaced)
{
  PKCS12 *pkcs12 = _load_bundle("server-protected-asdfg.p12");
  /* Keep the encryption password valid but make its MAC verification fail. */
  cr_assert_eq(PKCS12_set_mac(pkcs12, "different-mac-password", -1, NULL, 0, 1, EVP_sha256()), 1);
  _assert_parse(pkcs12, "asdfg", 0);
  cr_assert_eq(PKCS12_verify_mac(pkcs12, "different-mac-password", -1), 1);
  PKCS12_free(pkcs12);
}

Test(openssl_pkcs12, passwordless_without_mac_and_ca_chain)
{
  PKCS12 *original = _load_bundle("server.p12");
  EVP_PKEY *private_key = NULL;
  X509 *cert = NULL;
  cr_assert_eq(syslog_ng_pkcs12_parse(original, "", &private_key, &cert, NULL), 1);
  STACK_OF(X509) *ca_list = sk_X509_new_null();
  cr_assert_not_null(ca_list);
  cr_assert_gt(sk_X509_push(ca_list, cert), 0);

  PKCS12 *pkcs12 = PKCS12_create("", NULL, private_key, cert, ca_list,
                                 NID_aes_256_cbc, NID_aes_256_cbc, 1, -1, 0);
  cr_assert_not_null(pkcs12);
  cr_assert_eq(PKCS12_mac_present(pkcs12), 0);
  _assert_parse(pkcs12, "", 1);
  _assert_parse(pkcs12, NULL, 1);

  EVP_PKEY *parsed_key = NULL;
  X509 *parsed_cert = NULL;
  STACK_OF(X509) *parsed_ca_list = NULL;
  cr_assert_eq(syslog_ng_pkcs12_parse(pkcs12, "", &parsed_key, &parsed_cert, &parsed_ca_list), 1);
  cr_assert_eq(sk_X509_num(parsed_ca_list), 1);
  cr_assert_eq(X509_cmp(sk_X509_value(parsed_ca_list, 0), cert), 0);
  EVP_PKEY_free(parsed_key);
  X509_free(parsed_cert);
  sk_X509_pop_free(parsed_ca_list, X509_free);
  PKCS12_free(pkcs12);
  sk_X509_free(ca_list);
  EVP_PKEY_free(private_key);
  X509_free(cert);
  PKCS12_free(original);
}
