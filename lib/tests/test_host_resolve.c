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

/* NOTE: this is not run automatically in make check as it relies on DNS,
 * you have to invoke it manually if your workstation has internet
 * connectivity.
 */

#include "host-resolve.h"
#include "testutils.h"
#include "apphook.h"
#include "dnscache.h"
#include "gsocket.h"
#include "hostname.h"
#include "cfg.h"

#define HOST_RESOLVE_TESTCASE(x, ...) do { host_resolve_testcase_begin(domain_override, #x, #__VA_ARGS__); x(__VA_ARGS__); host_resolve_testcase_end(); } while(0)


#define host_resolve_testcase_begin(domain_override, func, args)    \
  do                                                            	\
    {                                                           	\
      testcase_begin("%s(%s)", func, args);                     	\
      dns_cache_thread_init();						\
      host_resolve_options_defaults(&host_resolve_options);		\
      host_resolve_options_init(&host_resolve_options, configuration);	\
      hostname_reinit(NULL);						\
    }                                                           	\
  while (0)

#define host_resolve_testcase_end()                             \
  do                                                            \
    {                                                           \
      host_resolve_options_destroy(&host_resolve_options);	\
      dns_cache_thread_deinit();				\
      testcase_end();                                           \
    }                                                           \
  while (0)

static HostResolveOptions host_resolve_options;

static void
assert_sockaddr_to_hostname(const gchar *ip, const gchar *expected)
{
  GSockAddr *sa;
  gchar result[256];
  gsize result_len = sizeof(result);

  sa = g_sockaddr_inet_new(ip, 0);
  resolve_sockaddr_to_hostname(result, &result_len, sa, &host_resolve_options);
  g_sockaddr_unref(sa);

  assert_string(result, expected, "resolved name mismatch");
}

static void
assert_sockaddr_to_short_hostname(const gchar *ip, const gchar *expected)
{
  host_resolve_options.use_fqdn = FALSE;
  assert_sockaddr_to_hostname(ip, expected);
}

static void
assert_sockaddr_to_fqdn_hostname(const gchar *ip, const gchar *expected)
{
  host_resolve_options.use_fqdn = TRUE;
  assert_sockaddr_to_hostname(ip, expected);
}

static void
assert_hostname_to_sockaddr(const gchar *hostname, const gchar *expected_ip)
{
  GSockAddr *sa;
  struct in_addr ina;
  gchar ip[64];
  gboolean result;

  sa = g_sockaddr_inet_new("0.0.0.0", 0);
  result = resolve_hostname_to_sockaddr(&sa, hostname);
  ina = g_sockaddr_inet_get_address(sa);
  g_inet_ntoa(ip, sizeof(ip), ina);
  g_sockaddr_unref(sa);

  assert_true(result, "unexpected error return");
  assert_string(ip, expected_ip, "resolved address mismatch");
}

static void
assert_hostname_to_sockaddr_fails(const gchar *hostname)
{
  GSockAddr *sa;
  gboolean result;

  sa = g_sockaddr_inet_new("0.0.0.0", 0);
  result = resolve_hostname_to_sockaddr(&sa, hostname);
  g_sockaddr_unref(sa);

  assert_false(result, "unexpected success returned");
}

static void
assert_hostname_to_hostname(const gchar *hostname, const gchar *expected)
{
  gchar result[256];
  gsize result_len = sizeof(result);

  resolve_hostname_to_hostname(result, &result_len, hostname, &host_resolve_options);
  assert_string(result, expected, "hostname to hostname result mismatch");
}


#define for_all_resolve_cases() 												\
  for (host_resolve_options.use_dns_cache = 0; host_resolve_options.use_dns_cache < 2; host_resolve_options.use_dns_cache++)	\
    for (host_resolve_options.normalize_hostnames = 0; host_resolve_options.normalize_hostnames < 2; host_resolve_options.normalize_hostnames++)

static void
test_resolvable_ip_results_in_hostname(void)
{
  host_resolve_options.use_dns = TRUE;
  for_all_resolve_cases()
    {
      /* a.root-servers.net, will probably not go away as its IP is registered to bind hints file */
      assert_sockaddr_to_short_hostname("198.41.0.4", "a");
      assert_sockaddr_to_fqdn_hostname("198.41.0.4", "a.root-servers.net");
    }
}

static void
test_unresolvable_ip_results_in_ip(void)
{
  fprintf(stderr, "The testcase %s takes a lot of time, it is advisable to turn it\n"
                  "off for short iterations and reenable it at the end of the session.\n"
                  "The easiest way to disable it is to comment out its invocation that\n"
                  "looks like HOST_RESOLVE_TESTCASE(test_unresolvable_ip_results_in_ip);\n"
                  "But please, please, please don't commit the disabling of that testcase.\n",
                  __FUNCTION__);

  host_resolve_options.use_dns = TRUE;
  for_all_resolve_cases()
    {
      /* 198.41.0.251 is on the same network as a.root-servers.net, but is
       * not resolvable as of now. It is a good candidate for the negative tests
       * as it responds quite fast.
       *
       * NOTE: this might become resolve once in the future, in that case
       * this testcase will fail.  Search for an IP address that has a
       * responding DNS server but has no A record.
       */
      assert_sockaddr_to_short_hostname("198.41.0.251", "198.41.0.251");
      assert_sockaddr_to_fqdn_hostname("198.41.0.251", "198.41.0.251");
    }
}

static void
test_sockaddr_without_dns_resolution_results_in_ip(void)
{
  host_resolve_options.use_dns = FALSE;
  for_all_resolve_cases()
    {
      /* a.root-servers.net, will probably not go away as its IP is registered to bind hints file */
      assert_sockaddr_to_short_hostname("198.41.0.4", "198.41.0.4");
      assert_sockaddr_to_fqdn_hostname("198.41.0.4", "198.41.0.4");
    }
}

static void
test_resolve_sockaddr_to_hostname(void)
{
  HOST_RESOLVE_TESTCASE(test_resolvable_ip_results_in_hostname);
  HOST_RESOLVE_TESTCASE(test_unresolvable_ip_results_in_ip);
  HOST_RESOLVE_TESTCASE(test_sockaddr_without_dns_resolution_results_in_ip);
}

static void
test_resolvable_hostname_results_in_sockaddr(void)
{
  assert_hostname_to_sockaddr("a.root-servers.net", "198.41.0.4");
}

static void
test_unresolvable_hostname_results_in_error(void)
{
  assert_hostname_to_sockaddr_fails("foo.bar.baz");
}

static void
test_resolve_hostname_to_sockaddr(void)
{
  HOST_RESOLVE_TESTCASE(test_resolvable_hostname_results_in_sockaddr);
  HOST_RESOLVE_TESTCASE(test_unresolvable_hostname_results_in_error);
}

static void
test_short_hostname_is_converted_to_fqdn_if_use_fqdn_is_set(void)
{
  host_resolve_options.use_fqdn = TRUE;

  /* force the use of custom domain to make asserts easier. the
   * non-custom-domain case is tested by test-hostname.c */

  hostname_reinit("bardomain");
  assert_hostname_to_hostname("foo", "foo.bardomain");
}

static void
test_fqdn_hostname_is_converted_to_short_if_use_fqdn_is_unset(void)
{
  host_resolve_options.use_fqdn = FALSE;

  assert_hostname_to_hostname("foo.bardomain", "foo");
}

static void
test_hostname_is_normalized_if_normalize_hostnames_is_set(void)
{
  host_resolve_options.use_fqdn = TRUE;
  host_resolve_options.normalize_hostnames = TRUE;

  assert_hostname_to_hostname("Foo.BarDomain", "foo.bardomain");
}

static void
test_resolve_hostname_to_hostname(void)
{
  HOST_RESOLVE_TESTCASE(test_short_hostname_is_converted_to_fqdn_if_use_fqdn_is_set);
  HOST_RESOLVE_TESTCASE(test_fqdn_hostname_is_converted_to_short_if_use_fqdn_is_unset);
  HOST_RESOLVE_TESTCASE(test_hostname_is_normalized_if_normalize_hostnames_is_set);
}

int
main(int argc, char *argv[])
{
  app_startup();

  configuration = cfg_new(VERSION_VALUE);
  test_resolve_hostname_to_hostname();
  test_resolve_hostname_to_sockaddr();
  test_resolve_sockaddr_to_hostname();
  cfg_free(configuration);
  return 0;
}
