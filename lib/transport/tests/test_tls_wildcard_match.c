/*
 * Copyright (c) 2024 One Identity LLC.
 * Copyright (c) 2024 Franco Fichtner
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


#include <criterion/criterion.h>

#include "transport/tls-verifier.h"

#include <openssl/x509.h>
#include <openssl/x509v3.h>

TestSuite(tls_wildcard, .init = NULL, .fini = NULL);

Test(tls_wildcard, test_wildcard_match_pattern_acceptance)
{
  cr_assert_eq(tls_wildcard_match("test", "test"), TRUE);
  cr_assert_eq(tls_wildcard_match("test", "*"), TRUE);
  cr_assert_eq(tls_wildcard_match("test", "t*t"), TRUE);
  cr_assert_eq(tls_wildcard_match("test", "t*"), TRUE);
  cr_assert_eq(tls_wildcard_match("", ""), TRUE);
  cr_assert_eq(tls_wildcard_match("test.one", "test.one"), TRUE);
  cr_assert_eq(tls_wildcard_match("test.one.two", "test.one.two"), TRUE);
  cr_assert_eq(tls_wildcard_match("192.0.2.0", "192.0.2.0"), TRUE);
  cr_assert_eq(tls_wildcard_match("2001:0000:130F:0000:0000:09C0:876A:130B", "2001:0000:130F:0000:0000:09C0:876A:130B"),
               TRUE);
  cr_assert_eq(tls_wildcard_match("2001:0000:130F:0000:0000:09C0:876A:130B", "2001:0:130F:0:0:9C0:876A:130B"), TRUE);
  cr_assert_eq(tls_wildcard_match("2001:0:130F:0:0:9C0:876A:130B", "2001:0000:130F:0000:0000:09C0:876A:130B"), TRUE);
  cr_assert_eq(tls_wildcard_match("2001:0000:130F::09C0:876A:130B", "2001:0000:130F:0000:0000:09C0:876A:130B"), TRUE);
  cr_assert_eq(tls_wildcard_match("2001:0000:130F:0000:0000:09C0:876A:130B", "2001:0000:130F::09C0:876A:130B"), TRUE);
  cr_assert_eq(tls_wildcard_match("2001:0000:130F:0000:0000:09C0:876A:130B", "2001:0:130F::9C0:876A:130B"), TRUE);
  cr_assert_eq(tls_wildcard_match("2001:0:130F::9C0:876A:130B", "2001:0000:130F:0000:0000:09C0:876A:130B"), TRUE);
}

Test(tls_wildcard, test_wildcard_match_wildcard_rejection)
{
  cr_assert_eq(tls_wildcard_match("test", "**"), FALSE);
  cr_assert_eq(tls_wildcard_match("test", "*es*"), FALSE);
  cr_assert_eq(tls_wildcard_match("test", "t*?"), FALSE);
}

Test(tls_wildcard, test_wildcard_match_pattern_rejection)
{
  cr_assert_eq(tls_wildcard_match("test", "tset"), FALSE);
  cr_assert_eq(tls_wildcard_match("test", "set"), FALSE);
  cr_assert_eq(tls_wildcard_match("", "*"), FALSE);
  cr_assert_eq(tls_wildcard_match("test", ""), FALSE);
  cr_assert_eq(tls_wildcard_match("test.two", "test.one"), FALSE);
}

Test(tls_wildcard, test_wildcard_match_format_rejection)
{
  cr_assert_eq(tls_wildcard_match("test.two", "test.*"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.two", "test.t*o"), FALSE);
  cr_assert_eq(tls_wildcard_match("test", "test.two"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.two", "test"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.one.two", "test.one"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.one", "test.one.two"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.three", "three.test"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.one.two", "test.one.*"), FALSE);
}

Test(tls_wildcard, test_wildcard_match_complex_rejection)
{
  cr_assert_eq(tls_wildcard_match("test.two", "test.???"), FALSE);
  cr_assert_eq(tls_wildcard_match("test.one.two", "test.one.?wo"), FALSE);
}

Test(tls_wildcard, test_ip_wildcard_rejection)
{
  cr_assert_eq(tls_wildcard_match("192.0.2.0", "*.0.2.0"), FALSE);
  cr_assert_eq(tls_wildcard_match("2001:0000:130F:0000:0000:09C0:876A:130B", "*:0000:130F:0000:0000:09C0:876A:130B"),
               FALSE);
  cr_assert_eq(tls_wildcard_match("2001:0:130F::9C0:876A:130B", "*:0000:130F:0000:0000:09C0:876A:130B"), FALSE);
}

Test(tls_wildcard, test_case_insensivity)
{
  cr_assert_eq(tls_wildcard_match("test", "TEST"), TRUE);
  cr_assert_eq(tls_wildcard_match("TEST", "test"), TRUE);
  cr_assert_eq(tls_wildcard_match("TeST", "TEst"), TRUE);
  cr_assert_eq(tls_wildcard_match("test.one", "test.ONE"), TRUE);
  cr_assert_eq(tls_wildcard_match("test.TWO", "test.two"), TRUE);
  cr_assert_eq(tls_wildcard_match("test.three", "*T.three"), TRUE);
  cr_assert_eq(tls_wildcard_match("2001:0000:130F:0000:0000:09C0:876A:130B", "2001:0000:130f:0000:0000:09c0:876a:130b"),
               TRUE);
}

static void
_add_dns_name(GENERAL_NAMES *alt_names, const guchar *dns_name, gint dns_name_len)
{
  GENERAL_NAME *gen_name = GENERAL_NAME_new();
  ASN1_IA5STRING *dns = ASN1_IA5STRING_new();

  cr_assert(ASN1_STRING_set(dns, dns_name, dns_name_len));
  GENERAL_NAME_set0_value(gen_name, GEN_DNS, dns);
  cr_assert(sk_GENERAL_NAME_push(alt_names, gen_name));
}

static X509 *
_create_cert(const guchar *cn, gint cn_len, const guchar *san_dns, gint san_dns_len)
{
  X509 *cert = X509_new();
  cr_assert_not_null(cert);

  X509_NAME *name = X509_get_subject_name(cert);
  cr_assert(X509_NAME_add_entry_by_NID(name, NID_commonName, V_ASN1_UTF8STRING, cn, cn_len, -1, 0));

  if (san_dns)
    {
      GENERAL_NAMES *alt_names = sk_GENERAL_NAME_new_null();

      _add_dns_name(alt_names, san_dns, san_dns_len);
      cr_assert(X509_add1_ext_i2d(cert, NID_subject_alt_name, alt_names, 0, X509V3_ADD_DEFAULT));
      GENERAL_NAMES_free(alt_names);
    }

  return cert;
}

/* "good.example.com\0.evil.example.net": a CA validates the part it can see
 * (evil.example.net), a C string comparison sees only good.example.com */
static const guchar nul_name[] = "good.example.com\0.evil.example.net";

Test(tls_verify_certificate_name, test_common_name_matches_without_subject_alt_name)
{
  X509 *cert = _create_cert((const guchar *) "good.example.com", -1, NULL, 0);

  cr_assert(tls_verify_certificate_name(cert, "good.example.com"));
  cr_assert_not(tls_verify_certificate_name(cert, "other.example.com"));
  X509_free(cert);
}

Test(tls_verify_certificate_name, test_common_name_with_embedded_nul_does_not_match)
{
  X509 *cert = _create_cert(nul_name, sizeof(nul_name) - 1, NULL, 0);

  cr_assert_not(tls_verify_certificate_name(cert, "good.example.com"));
  X509_free(cert);
}

Test(tls_verify_certificate_name, test_subject_alt_name_matches)
{
  X509 *cert = _create_cert((const guchar *) "ignored", -1, (const guchar *) "good.example.com", 16);

  cr_assert(tls_verify_certificate_name(cert, "good.example.com"));
  X509_free(cert);
}

Test(tls_verify_certificate_name, test_subject_alt_name_with_embedded_nul_does_not_match)
{
  X509 *cert = _create_cert((const guchar *) "ignored", -1, nul_name, sizeof(nul_name) - 1);

  cr_assert_not(tls_verify_certificate_name(cert, "good.example.com"));
  X509_free(cert);
}

Test(tls_verify_certificate_name, test_subject_alt_name_takes_precedence_over_matching_common_name)
{
  X509 *cert = _create_cert((const guchar *) "good.example.com", -1,
                            (const guchar *) "other.example.com", sizeof("other.example.com") - 1);

  cr_assert_not(tls_verify_certificate_name(cert, "good.example.com"));
  X509_free(cert);
}

Test(tls_verify_certificate_name, test_subject_alt_name_with_embedded_nul_does_not_fall_back_to_common_name)
{
  X509 *cert = _create_cert((const guchar *) "good.example.com", -1, nul_name, sizeof(nul_name) - 1);

  cr_assert_not(tls_verify_certificate_name(cert, "good.example.com"));
  X509_free(cert);
}

Test(tls_verify_certificate_name, test_oversized_subject_alt_name_does_not_fall_back_to_common_name)
{
  guchar oversized_name[256];
  memset(oversized_name, 'a', sizeof(oversized_name));
  X509 *cert = _create_cert((const guchar *) "good.example.com", -1, oversized_name, sizeof(oversized_name));

  cr_assert_not(tls_verify_certificate_name(cert, "good.example.com"));
  X509_free(cert);
}

Test(tls_verify_certificate_name, test_valid_subject_alt_name_after_embedded_nul_still_matches)
{
  X509 *cert = _create_cert((const guchar *) "ignored", -1, nul_name, sizeof(nul_name) - 1);
  GENERAL_NAMES *alt_names = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
  cr_assert_not_null(alt_names);

  _add_dns_name(alt_names, (const guchar *) "good.example.com", sizeof("good.example.com") - 1);
  cr_assert(X509_add1_ext_i2d(cert, NID_subject_alt_name, alt_names, 0, X509V3_ADD_REPLACE));
  GENERAL_NAMES_free(alt_names);

  cr_assert(tls_verify_certificate_name(cert, "good.example.com"));
  X509_free(cert);
}
