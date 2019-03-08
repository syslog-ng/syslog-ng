/*
 * Copyright (c) 2015-2019 Balabit
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
 */
#include "scanner/kv-scanner/kv-scanner.h"
#include "linux-audit-parser.h"
#include "apphook.h"

#include <criterion/criterion.h>

KVScanner kv_scanner;

static void
assert_no_more_tokens(void)
{
  cr_assert_not(kv_scanner_scan_next(&kv_scanner), "kv_scanner is expected to return no more key-value pairs");
}

static void
scan_next_token(void)
{
  cr_assert(kv_scanner_scan_next(&kv_scanner),  "kv_scanner is expected to return TRUE for scan_next");
}

static void
assert_current_key_is(const gchar *expected_key)
{
  const gchar *key = kv_scanner_get_current_key(&kv_scanner);

  cr_assert_str_eq(key, expected_key, "current key mismatch, key: %s, expected_key: %s", key, expected_key);
}

static void
assert_current_value_is(const gchar *expected_value)
{
  const gchar *value = kv_scanner_get_current_value(&kv_scanner);

  cr_assert_str_eq(value, expected_value, "current value mismatch, value: %s, expected_value: %s",
                   value, expected_value);
}

static void
assert_current_kv_is(const gchar *expected_key, const gchar *expected_value)
{
  assert_current_key_is(expected_key);
  assert_current_value_is(expected_value);
}

static void
assert_next_kv_is(const gchar *expected_key, const gchar *expected_value)
{
  scan_next_token();
  assert_current_kv_is(expected_key, expected_value);
}

void
setup(void)
{
  app_startup();
  kv_scanner_init(&kv_scanner, '=', 0, FALSE);
  kv_scanner_set_transform_value(&kv_scanner, parse_linux_audit_style_hexdump);
}

void
teardown(void)
{
  app_shutdown();
}

Test(linux_audit_scanner, test_audit_style_hex_dump_is_decoded)
{
  kv_scanner_input(&kv_scanner, "proctitle=41607E");
  assert_next_kv_is("proctitle", "41607E");
  assert_no_more_tokens();

  kv_scanner_input(&kv_scanner, "proctitle=412042");
  assert_next_kv_is("proctitle", "A B");
  assert_no_more_tokens();

  kv_scanner_input(&kv_scanner, "proctitle=41204");
  assert_next_kv_is("proctitle", "41204");
  assert_no_more_tokens();

  kv_scanner_input(&kv_scanner, "proctitle=C3A17276C3AD7A74C5B172C59174C3BC6BC3B67266C3BA72C3B367C3A970");
  assert_next_kv_is("proctitle", "árvíztűrőtükörfúrógép");
  assert_no_more_tokens();

  kv_scanner_input(&kv_scanner, "proctitle=2F62696E2F7368002D65002F6574632F696E69742E642F706F737466697800737461747573");
  assert_next_kv_is("proctitle", "/bin/sh\t-e\t/etc/init.d/postfix\tstatus");
  assert_no_more_tokens();

  kv_scanner_input(&kv_scanner, "a1=2F62696E2F7368202D6C");
  assert_next_kv_is("a1", "/bin/sh -l");
  assert_no_more_tokens();
}

TestSuite(linux_audit_scanner, .init = setup, .fini = teardown);
