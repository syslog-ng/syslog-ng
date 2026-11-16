/*
 * Copyright (c) 2012-2018 Balabit
 * Copyright (c) 2012-2015 Balázs Scheidler <balazs.scheidler@balabit.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <criterion/criterion.h>
#include "libtest/cr_template.h"
#include "compat/openssl_support.h"

#include "apphook.h"
#include "cfg.h"

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/crypto.h>
#include <openssl/provider.h>
#endif

void
setup(void)
{
  app_startup();
  init_template_tests();
  cfg_load_module(configuration, "cryptofuncs");
}

void
teardown(void)
{
  deinit_template_tests();
  app_shutdown();
}

TestSuite(cryptofuncs, .init = setup, .fini = teardown);

Test(cryptofuncs, test_hash)
{
  assert_template_format("$(sha1 foo)", "0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33");
  assert_template_format("$(sha1 bar)", "62cdb7020ff920e5aa642c3d4066950dd1f01f4d");
  assert_template_format("$(md5 foo)", "acbd18db4cc2f85cedef654fccc4a4d8");
  assert_template_format("$(hash foo)", "2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae");
#if SYSLOG_NG_HAVE_DECL_DIGEST_MD4
  assert_template_format("$(md4 foo)", "0ac6700c491d70fb8650940b1ca1e4b2");
#else
  assert_template_failure("$(md4 foo)",
                          "MD4 hash function is not available starting with OpenSSL 3.0.");
#endif
  assert_template_format("$(sha256 foo)", "2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae");
  assert_template_format("$(sha512 foo)",
                         "f7fbba6e0636f890e56fbbf3283e524c6fa3204ae298382d624741d0dc6638326e282c41be5e4254d8820772c5518a2c5a8c0c7f7eda19594a7eb539453e1ed7");
  assert_template_failure("$(sha1)", "$(hash) parsing failed, invalid number of arguments");
  assert_template_format("$(sha1 --length 5 foo)", "0beec");
  assert_template_format("$(sha1 -l 5 foo)", "0beec");
  assert_template_failure("$(sha1 --length 5)", "$(hash) parsing failed, invalid number of arguments");
  assert_template_failure("$(sha1 --length 5)", "$(hash) parsing failed, invalid number of arguments");
  assert_template_failure("$(sha1 ${missingbrace)", "Invalid macro, '}' is missing, error_pos='14'");
  assert_template_failure("$(sha1 --length invalid_length_specification foo)",
                          "Cannot parse integer value");
  assert_template_format("$(sha1 --length 99999 foo)", "0beec7b5ea3f0fdbc95d0dd47f3c5bc275da8a33");
  assert_template_format("$(sha1 foo bar)", "8843d7f92416211de9ebb963ff4ce28125932878");
  assert_template_format("$(sha1 \"foo bar\")", "3773dea65156909838fa6c22825cafe090ff8030");
  assert_template_format("$(md5 $(sha1 foo) bar)", "196894290a831b2d2755c8de22619a97");
}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
static OSSL_LIB_CTX *crypto_context;
static OSSL_LIB_CTX *original_crypto_context;
static OSSL_PROVIDER *crypto_provider;

static void
_setup_with_crypto_context(void)
{
  crypto_context = OSSL_LIB_CTX_new();
  cr_assert_not_null(crypto_context);
  original_crypto_context = OSSL_LIB_CTX_set0_default(crypto_context);
  cr_assert_not_null(original_crypto_context);
}

static void
_teardown_with_crypto_context(void)
{
  if (original_crypto_context)
    OSSL_LIB_CTX_set0_default(original_crypto_context);
  if (crypto_provider)
    OSSL_PROVIDER_unload(crypto_provider);
  OSSL_LIB_CTX_free(crypto_context);
}

Test(cryptofuncs, test_md5_with_fips_default_properties,
     .init = _setup_with_crypto_context, .fini = _teardown_with_crypto_context)
{
  /* Exercise property selection without requiring an installed FIPS provider. */
  crypto_provider = OSSL_PROVIDER_load(crypto_context, "default");
  cr_assert_not_null(crypto_provider);
  cr_assert(EVP_default_properties_enable_fips(crypto_context, TRUE));

  /* MD5 may ignore fips=yes locally; approved digests must still obey it. */
  assert_template_format("$(md5 foo)", "acbd18db4cc2f85cedef654fccc4a4d8");
  assert_template_format("$(md5 --length 5 foo)", "acbd1");
  cr_assert(EVP_default_properties_is_fips_enabled(crypto_context));
  assert_template_failure("$(sha256 foo)", "$(hash) parsing failed, unknown or unavailable digest type");
  assert_template_failure("$(hash foo)", "$(hash) parsing failed, unknown or unavailable digest type");
}

Test(cryptofuncs, test_md5_unavailable_digest_fails_compilation,
     .init = _setup_with_crypto_context, .fini = _teardown_with_crypto_context)
{
  /* Prevent automatic loading of the default provider. */
  crypto_provider = OSSL_PROVIDER_load(crypto_context, "null");
  cr_assert_not_null(crypto_provider);

  assert_template_failure("$(md5 foo)", "$(hash) parsing failed, unknown or unavailable digest type");
}
#endif
