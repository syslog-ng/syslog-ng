/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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
#include "hostname.h"
#include "testutils.h"
#include "apphook.h"
#include <string.h>

/*
 * NOTE: on WRAP_GETHOSTNAME below
 *
 * If WRAP_GETHOSTNAME is 1, this test will mock the gethostname() call in
 * order to run on all systems.  When gethostname() is mocked, it will
 * always return an FQDN as a hostname.  This in turn means, that the use of
 * DNS in detecting the FQDN is completely avoided.
 *
 * This means, that if WRAP_GETHOSTNAME is set to 1, it will run on all
 * systems, but some of the production code will not be tested.  If it is
 * set to 0, it'll only run on a host whose name is "bzorp.balabit", which
 * happens to be my notebook.
 *
 * I was thinking of mocking the DNS calls too, but that gets more and more
 * difficult, so I was left with this macro that I can change whenever I'm
 * working on the DNS related parts of the hostname.c file.
 */

#define WRAP_GETHOSTNAME 1

#ifdef WRAP_GETHOSTNAME
static int (*__wrap_gethostname)(char *buf, size_t buflen);

#define gethostname __wrap_gethostname
#include "hostname.c"
#undef gethostname

static int
__fqdn_gethostname(char *buf, size_t buflen)
{
  strncpy(buf, "bzorp.balabit", buflen);
  return 0;
}
#endif

static void
assert_hostname_fqdn(const gchar *expected)
{
  const gchar *host;

  host = get_local_hostname_fqdn();
  assert_string(host, expected, "hostname values mismatch");
}

static void
assert_hostname_short(const gchar *expected)
{
  const gchar *host;

  host = get_local_hostname_short();
  assert_string(host, expected, "hostname values mismatch");
}

static void
assert_fqdn_conversion(gchar *hostname, const gchar *expected)
{
  gchar buf[256];

  g_strlcpy(buf, hostname, sizeof(buf));
  convert_hostname_to_fqdn(buf, sizeof(buf));
  assert_string(buf, expected, "hostname values mismatch");
}

static void
assert_short_conversion(gchar *hostname, const gchar *expected)
{
  gchar buf[256];

  g_strlcpy(buf, hostname, sizeof(buf));
  convert_hostname_to_short_hostname(buf, sizeof(buf));
  assert_string(buf, expected, "hostname values mismatch");
}

#define HOSTNAME_TESTCASE_WITH_DOMAIN(domain_override, x, ...) do { hostname_testcase_begin(domain_override, #x, #__VA_ARGS__); x(__VA_ARGS__); hostname_testcase_end(); } while(0)
#define HOSTNAME_TESTCASE(x, ...) HOSTNAME_TESTCASE_WITH_DOMAIN(NULL, x, __VA_ARGS__)


#ifdef WRAP_GETHOSTNAME
#define wrap_gethostname() (__wrap_gethostname = __fqdn_gethostname)
#else
#endif

#define hostname_testcase_begin(domain_override, func, args)    \
  do                                                            \
    {                                                           \
      testcase_begin("%s(%s)", func, args);                     \
      wrap_gethostname();					\
      hostname_reinit(domain_override);				\
    }                                                           \
  while (0)

#define hostname_testcase_end()                                 \
  do                                                            \
    {                                                           \
      testcase_end();                                           \
    }                                                           \
  while (0)


static void
test_without_domain_override_the_domain_in_the_hostname_is_used(void)
{
  assert_fqdn_conversion("foo.bar", "foo.bar");
}

static void
test_without_domain_override_the_local_domain_is_used_when_converting_a_short_hostname_to_fqdn(void)
{
  assert_fqdn_conversion("foo", "foo.balabit");
}

static void
test_without_domain_override_short_conversion_removes_any_domain_name(void)
{
  assert_short_conversion("foo", "foo");
  assert_short_conversion("foo.bar", "foo");
  assert_short_conversion("foo.bardomain", "foo");
}

static void
test_domain_override_short_conversion_removes_any_domain_name(void)
{
  assert_short_conversion("foo", "foo");
  assert_short_conversion("foo.bar", "foo");
  assert_short_conversion("foo.bardomain", "foo");
}

static void
test_system_host_and_domain_name_are_detected_properly(void)
{
  assert_hostname_fqdn("bzorp.balabit");
  assert_hostname_short("bzorp");
  assert_fqdn_conversion("bzorp", "bzorp.balabit");
  assert_fqdn_conversion("bzorp.balabit", "bzorp.balabit");
}

static void
test_domain_override_replaces_domain_detected_on_the_system(void)
{
  assert_hostname_fqdn("bzorp.bardomain");
  assert_hostname_short("bzorp");
  assert_fqdn_conversion("bzorp", "bzorp.bardomain");
  assert_fqdn_conversion("bzorp.balabit", "bzorp.bardomain");
  assert_fqdn_conversion("foo", "foo.bardomain");
  assert_fqdn_conversion("foo.bar", "foo.bardomain");
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  HOSTNAME_TESTCASE(test_without_domain_override_the_domain_in_the_hostname_is_used);
  HOSTNAME_TESTCASE(test_without_domain_override_the_local_domain_is_used_when_converting_a_short_hostname_to_fqdn);
  HOSTNAME_TESTCASE(test_without_domain_override_short_conversion_removes_any_domain_name);
  HOSTNAME_TESTCASE_WITH_DOMAIN("bardomain", test_domain_override_short_conversion_removes_any_domain_name);
  HOSTNAME_TESTCASE(test_system_host_and_domain_name_are_detected_properly);
  HOSTNAME_TESTCASE_WITH_DOMAIN("bardomain", test_domain_override_replaces_domain_detected_on_the_system);
  return 0;
}
