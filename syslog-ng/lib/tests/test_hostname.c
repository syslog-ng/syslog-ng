/*
 * Copyright (c) 2002-2018 Balabit
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
#include <criterion/criterion.h>
#include <criterion/parameterized.h>
#include "apphook.h"

#include <string.h>
#include <unistd.h>

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

#else
#include "hostname.c"
#endif

#ifdef WRAP_GETHOSTNAME
#define wrap_gethostname() (__wrap_gethostname = __fqdn_gethostname)
#else
#define wrap_gethostname()
#endif

/* this is hostname-unix specific and will probably be made conditional once the win32 bits are in */

/* NOTE: we are testing a private function */
static const gchar *
_invoke_extract_fqdn_from_hostent(gchar *primary_host, gchar **aliases)
{
  struct hostent hostent;

  memset(&hostent, 0, sizeof(hostent));
  hostent.h_name = primary_host;
  hostent.h_aliases = aliases;
  return _extract_fqdn_from_hostent(&hostent);
}

typedef struct _HostNameList
{
  gchar *domain_override;
  gchar *host_name;
  gchar *expected;
} HostNameList;

ParameterizedTestParameters(test_hostname, test_hostname_fqdn_conversion)
{
  static HostNameList host_name_list[] =
  {
    {NULL, "foo.bar", "foo.bar"},
    {NULL, "foo", "foo.balabit"},
    {NULL, "bzorp", "bzorp.balabit"},
    {NULL, "bzorp.balabit", "bzorp.balabit"},
    {"bardomain", "bzorp", "bzorp.bardomain"},
    {"bardomain", "bzorp.balabit", "bzorp.bardomain"},
    {"bardomain", "foo", "foo.bardomain"},
    {"bardomain", "foo.bar", "foo.bardomain"}
  };

  return cr_make_param_array(HostNameList, host_name_list,
                             sizeof(host_name_list) / sizeof(host_name_list[0]));
}

ParameterizedTest(HostNameList *host_name_list, test_hostname, test_hostname_fqdn_conversion)
{
  gchar buf[256];

  wrap_gethostname();
  hostname_reinit(host_name_list->domain_override);

  g_strlcpy(buf, host_name_list->host_name, sizeof(buf));
  convert_hostname_to_fqdn(buf, sizeof(buf));
  cr_assert_str_eq(buf, host_name_list->expected, "hostname values mismatch");
}

ParameterizedTestParameters(test_hostname, test_hostname_short_conversion)
{
  static HostNameList host_name_list[] =
  {
    {NULL, "foo", "foo"},
    {NULL, "foo.bar", "foo"},
    {NULL, "foo.bardomain", "foo"},
    {"bardomain", "foo", "foo"},
    {"bardomain", "foo.bar", "foo"},
    {"bardomain", "foo.bardomain", "foo"},
  };

  return cr_make_param_array(HostNameList, host_name_list,
                             sizeof(host_name_list) / sizeof(host_name_list[0]));
}

ParameterizedTest(HostNameList *host_name_list, test_hostname, test_hostname_short_conversion)
{
  gchar buf[256];

  wrap_gethostname();
  hostname_reinit(host_name_list->domain_override);

  g_strlcpy(buf, host_name_list->host_name, sizeof(buf));
  convert_hostname_to_short_hostname(buf, sizeof(buf));
  cr_assert_str_eq(buf, host_name_list->expected, "hostname values mismatch");
}

ParameterizedTestParameters(test_hostname, test_hostname_fqdn)
{
  static HostNameList host_name_list[] =
  {
    {NULL, NULL, "bzorp.balabit"},
    {"bardomain", NULL, "bzorp.bardomain"},
  };

  return cr_make_param_array(HostNameList, host_name_list,
                             sizeof(host_name_list) / sizeof(host_name_list[0]));
}

ParameterizedTest(HostNameList *host_name_list, test_hostname, test_hostname_fqdn)
{
  const gchar *host;

  wrap_gethostname();
  hostname_reinit(host_name_list->domain_override);

  host = get_local_hostname_fqdn();
  cr_assert_str_eq(host, host_name_list->expected, "hostname values mismatch");
}

ParameterizedTestParameters(test_hostname, test_hostname_short)
{
  static HostNameList host_name_list[] =
  {
    {NULL, NULL, "bzorp"},
    {"bardomain", NULL, "bzorp"},
  };

  return cr_make_param_array(HostNameList, host_name_list,
                             sizeof(host_name_list) / sizeof(host_name_list[0]));
}

ParameterizedTest(HostNameList *host_name_list, test_hostname, test_hostname_short)
{
  const gchar *host;

  wrap_gethostname();
  hostname_reinit(host_name_list->domain_override);

  host = get_local_hostname_short();
  cr_assert_str_eq(host, host_name_list->expected, "hostname values mismatch");
}

Test(test_hostname, test_extract_fqdn_from_hostent_uses_primary_name_if_it_is_an_fqdn)
{
  gchar *aliases[] = { "bzorp", "bzorp.lan", NULL };

  cr_assert_str_eq(_invoke_extract_fqdn_from_hostent("bzorp.balabit", aliases), "bzorp.balabit",
                   "_extract_fqdn didn't return the requested hostname");
}

Test(test_hostname, test_extract_fqdn_from_hostent_finds_the_first_fqdn_in_aliases_if_primary_is_short)
{
  gchar *aliases[] = { "bzorp", "bzorp.lan", NULL };

  cr_assert_str_eq(_invoke_extract_fqdn_from_hostent("bzorp", aliases), "bzorp.lan",
                   "_extract_fqdn didn't return the requested hostname");
}

Test(test_hostname, test_extract_fqdn_from_hostent_returns_NULL_when_no_fqdn_is_found)
{
  gchar *aliases[] = { "bzorp", "foobar", NULL };

  cr_assert_null(_invoke_extract_fqdn_from_hostent("bzorp", aliases),
                 "_extract_fqdn returned non-NULL when we expected failure");
}
