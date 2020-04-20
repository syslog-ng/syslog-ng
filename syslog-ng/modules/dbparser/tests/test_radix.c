/*
 * Copyright (c) 2008-2018 Balabit
 * Copyright (c) 2008-2015 Balázs Scheidler <balazs.scheidler@balabit.com>
 * Copyright (c) 2009 Marton Illes
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

#include "apphook.h"
#include "radix.h"
#include "messages.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#define RADIX_TEST_MAX_PATTERN 5
#define RADIX_TEST_MAX_NODE 5

typedef struct _radix_test_param
{
  const gchar *node_to_insert[RADIX_TEST_MAX_NODE];
  const gchar *key;
  const gchar *expected_pattern[RADIX_TEST_MAX_PATTERN];
} RadixTestParam;

void
insert_node_with_value(RNode *root, const gchar *key, const gpointer value)
{
  gchar *dup;

  /* NOTE: we need to duplicate the key as r_insert_node modifies its input
   * and it might be a read-only string literal */

  dup = g_strdup(key);
  r_insert_node(root, dup, value ? : (gpointer)key, NULL);
  g_free(dup);
}

void
insert_node(RNode *root, const gchar *key)
{
  insert_node_with_value(root, key, NULL);
}

void
test_search_value(RNode *root, const gchar *key, const gchar *expected_value)
{
  RNode *ret = r_find_node(root, (gchar *)key, strlen(key), NULL);

  if (expected_value)
    {
      cr_assert(ret, "node not found. key=%s\n", key);
      cr_expect_str_eq(ret->value, expected_value, "FAIL: returned value does not match expected: '%s' <> '%s'\n",
                       (gchar *) ret->value, expected_value);
    }
  else
    {
      cr_assert_not(ret, "found unexpected: '%s' => '%s'\n", key, (gchar *) ret->value);
    }
}

void
test_search(RNode *root, const gchar *key, gboolean expect)
{
  test_search_value(root, key, expect ? key : NULL);
}

void
test_search_matches(RNode *root, const gchar *key, const gchar *search_pattern[])
{
  GArray *matches = g_array_new(FALSE, TRUE, sizeof(RParserMatch));
  g_array_set_size(matches, 1);

  RParserMatch *match;
  const gchar *match_name;

  RNode *ret = r_find_node(root, (gchar *) key, strlen(key), matches);

  if (!search_pattern[0])
    {
      cr_expect_not(ret, "found unexpected: '%s' => '%s' matches: ", key, (gchar *) ret->value);
    }
  else
    {
      cr_assert(ret, "not found while expected: '%s' => none %s\n", key, search_pattern[0]);

      for (int i=0; search_pattern[i]; i+=2)
        {
          cr_assert_lt(i/2, matches->len, "not enough matches: %d => expecting %d", i, matches->len);

          const gchar *expected_name = search_pattern[i];
          const gchar *expected_value = search_pattern[i+1];

          match = &g_array_index(matches, RParserMatch, (i/2)+1);
          match_name = log_msg_get_value_name(match->handle, NULL);

          cr_expect_str_eq(match_name, expected_name,
                           "name does not match (key=%s): '%s' => expecting '%s'\n",
                           key, match_name, expected_name);

          if (!match->match)
            {
              cr_expect_eq(match->len, strlen(expected_value),
                           "value length does not match (key=%s): %d => expecting %zu\n",
                           key, match->len, strlen(expected_value));
              cr_expect_eq(strncmp(&key[match->ofs], expected_value, match->len), 0,
                           "value does not match (key=%s): %*s => expecting %s\n",
                           key, match->len, &key[match->ofs], expected_value);
            }
          else
            {
              cr_expect_str_eq(match->match, expected_value,
                               "value does not match (key=%s): '%s' => expecting '%s'\n",
                               key, match->match, expected_value);
            }
        }
    }

  for (gsize i = 0; i < matches->len; i++)
    {
      match = &g_array_index(matches, RParserMatch, i);
      if (match->match)
        {
          g_free(match->match);
        }
    }
  g_array_free(matches, TRUE);
}

void test_setup(void)
{
  app_startup();
  msg_init(TRUE);
}

void test_teardown(void)
{
  app_shutdown();
}

Test(dbparser, test_literals, .init = test_setup, .fini = test_teardown)
{
  RNode *root = r_new_node("", NULL);

  insert_node(root, "alma");
  insert_node(root, "korte");
  insert_node(root, "barack");
  insert_node(root, "dinnye");
  insert_node(root, "almafa");
  insert_node(root, "almabor");
  insert_node(root, "almafa2");
  insert_node(root, "ko");
  insert_node(root, "koros");
  insert_node(root, "koro");
  insert_node(root, "koromporkolt");
  insert_node(root, "korom");
  insert_node(root, "korozott");
  insert_node(root, "al");
  insert_node(root, "all");
  insert_node(root, "uj\nsor");

  test_search(root, "alma", TRUE);
  test_search(root, "korte", TRUE);
  test_search(root, "barack", TRUE);
  test_search(root, "dinnye", TRUE);
  test_search(root, "almafa", TRUE);
  test_search(root, "almabor", TRUE);
  test_search(root, "almafa2", TRUE);
  test_search(root, "ko", TRUE);
  test_search(root, "koros", TRUE);
  test_search(root, "koro", TRUE);
  test_search(root, "koromporkolt", TRUE);
  test_search(root, "korom", TRUE);
  test_search(root, "korozott", TRUE);
  test_search(root, "al", TRUE);
  test_search(root, "all", TRUE);
  test_search(root, "uj", FALSE);
  test_search(root, "uj\nsor", TRUE);
  test_search_value(root, "uj\r\nsor", "uj\nsor");

  test_search(root, "mmm", FALSE);
  test_search_value(root, "kor", "ko");
  test_search_value(root, "ko", "ko");
  test_search_value(root, "kort", "ko");
  test_search_value(root, "korti", "ko");
  test_search_value(root, "korte", "korte");
  test_search_value(root, "kortes", "korte");
  test_search_value(root, "koromp", "korom");
  test_search_value(root, "korompo", "korom");
  test_search_value(root, "korompor", "korom");
  test_search_value(root, "korompok", "korom");
  test_search_value(root, "korompa", "korom");
  test_search_value(root, "koromi", "korom");

  test_search(root, "qwqw", FALSE);

  r_free_node(root, NULL);
}

Test(dbparser, test_parsers, .init = test_setup, .fini = test_teardown)
{
  RNode *root = r_new_node("", NULL);

  /* FIXME: more parsers */
  insert_node(root, "a@@NUMBER@@aa@@@@");
  insert_node(root, "a@@ab");
  insert_node(root, "a@@a@@");

  insert_node(root, "a@@@NUMBER:szam0@");
  insert_node(root, "a@NUMBER:szamx@aaa");
  insert_node(root, "a@NUMBER@");
  insert_node(root, "a@NUMBER@aa");
  insert_node(root, "a@NUMBER:szamx@@@");

  insert_node(root, "baa@NUMBER@");
  insert_node(root, "bxa@NUMBER:szam5@");
  insert_node(root, "baa@@");
  insert_node(root, "baa@@@@");
  insert_node(root, "ba@@a");
  insert_node(root, "bc@@a");
  insert_node(root, "@@a@NUMBER:szam4@@@");
  insert_node(root, "@@a@NUMBER:szam4@b");
  insert_node(root, "@@a");
  insert_node(root, "@@");
  insert_node(root, "@@@@");

  insert_node(root, "xxx@ESTRING@");
  insert_node(root, "xxx@QSTRING@");
  insert_node(root, "xxx@STRING@");
  insert_node(root, "xxx@ANYSTRING@");
  insert_node(root, "xxx@ESTRING@x");
  insert_node(root, "xxx@QSTRING@x");
  insert_node(root, "xxx@STRING@x");
  insert_node(root, "xxx@ANYSTRING@x");
  insert_node(root, "AAA@NUMBER:invalid=@AAA");
  insert_node(root, "AAA@SET@AAA");
  insert_node(root, "AAA@SET:set@AAA");
  insert_node(root, "AAA@MACADDR@AAA");
  insert_node(root, "newline@NUMBER@\n2ndline\n");
  insert_node(root, "AAA@PCRE:set@AAA");

  test_search_value(root, "a@", NULL);
  test_search_value(root, "a@NUMBER@aa@@", "a@@NUMBER@@aa@@@@");
  test_search_value(root, "a@a", NULL);
  test_search_value(root, "a@ab", "a@@ab");
  test_search_value(root, "a@a@", "a@@a@@");
  test_search_value(root, "a@ax", NULL);

  test_search_value(root, "a@15555", "a@@@NUMBER:szam0@");

  /* CRLF sequence immediately after a parser, e.g. in the initial position */
  test_search_value(root, "newline123\r\n2ndline\n", "newline@NUMBER@\n2ndline\n");

  test_search_value(root, "a15555aaa", "a@NUMBER:szamx@aaa");
  test_search_value(root, "@a", "@@a");
  test_search_value(root, "@", "@@");
  test_search_value(root, "@@", "@@@@");

  r_free_node(root, NULL);
}

ParameterizedTestParameters(dbparser, test_radix_search_matches)
{
  static RadixTestParam parser_params[] =
  {
    /* test_ip_matches */
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key = "192.168.1.1 huhuhu",
      .expected_pattern = {"ip", "192.168.1.1", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key = "192.168.1.1. huhuhu",
      .expected_pattern = {"ip", "192.168.1.1", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key = "192.168.1huhuhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key = "192.168.1.huhuhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key = "192.168.1 huhuhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key = "192.168.1. huhuhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key = "192.168.1.1huhuhu",
      .expected_pattern = {"ip", "192.168.1.1", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="ABCD:EF01:2345:6789:ABCD:EF01:2345:6789 huhuhu",
      .expected_pattern = {"ip", "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="abcd:ef01:2345:6789:abcd:ef01:2345:6789 huhuhu",
      .expected_pattern = {"ip", "abcd:ef01:2345:6789:abcd:ef01:2345:6789", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key =":: huhuhu",
      .expected_pattern = {"ip", "::", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="0:0:0:0:0:0:13.1.68.3 huhuhu",
      .expected_pattern = {"ip", "0:0:0:0:0:0:13.1.68.3", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="::202.1.68.3 huhuhu",
      .expected_pattern = {"ip", "::202.1.68.3", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="2001:0DB8:0:CD30:: huhuhu",
      .expected_pattern = {"ip", "2001:0DB8:0:CD30::", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="ABCD:EF01:2345:6789:ABCD:EF01:2345:6789.huhuhu",
      .expected_pattern = {"ip", "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="abcd:ef01:2345:6789:abcd:ef01:2345:6789.huhuhu",
      .expected_pattern = {"ip", "abcd:ef01:2345:6789:abcd:ef01:2345:6789", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="::.huhuhu",
      .expected_pattern = {"ip", "::", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="0:0:0:0:0:0:13.1.68.3.huhuhu",
      .expected_pattern = {"ip", "0:0:0:0:0:0:13.1.68.3", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="::202.1.68.3.huhuhu",
      .expected_pattern = {"ip", "::202.1.68.3", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="2001:0DB8:0:CD30::.huhuhu",
      .expected_pattern = {"ip", "2001:0DB8:0:CD30::", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="1:2:3:4:5:6:7:8.huhuhu",
      .expected_pattern = {"ip", "1:2:3:4:5:6:7:8", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="1:2:3:4:5:6:7:8 huhuhu",
      .expected_pattern = {"ip", "1:2:3:4:5:6:7:8", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="1:2:3:4:5:6:7:8:huhuhu",
      .expected_pattern = {"ip", "1:2:3:4:5:6:7:8", NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="1:2:3:4:5:6:7 huhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="1:2:3:4:5:6:7.huhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="1:2:3:4:5:6:7:huhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="1:2:3:4:5:6:77777:8 huhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="1:2:3:4:5:6:1.2.333.4 huhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPvANY:ip@", NULL},
      .key ="v12345",
      .expected_pattern = {NULL}
    },
    /* test_ipv4_matches */
    {
      .node_to_insert = {"@IPv4:ipv4@", NULL},
      .key = "192.168.1.1 huhuhu",
      .expected_pattern = {"ipv4", "192.168.1.1", NULL}
    },
    {
      .node_to_insert = {"@IPv4:ipv4@", NULL},
      .key = "192.168.1.1. huhuhu",
      .expected_pattern = {"ipv4", "192.168.1.1", NULL}
    },
    {
      .node_to_insert = {"@IPv4:ipv4@", NULL},
      .key = "192.168.1.1.huhuhu",
      .expected_pattern = {"ipv4", "192.168.1.1", NULL}
    },
    {
      .node_to_insert = {"@IPv4:ipv4@", NULL},
      .key = "192.168.1.1.. huhuhu",
      .expected_pattern = {"ipv4", "192.168.1.1", NULL}
    },
    {
      .node_to_insert = {"@IPv4:ipv4@", NULL},
      .key = "192.168.1.1.2 huhuhu",
      .expected_pattern = {"ipv4", "192.168.1.1", NULL}
    },
    {
      .node_to_insert = {"@IPv4:ipv4@", NULL},
      .key = "192.168.1.1..huhuhu",
      .expected_pattern = {"ipv4", "192.168.1.1", NULL}
    },
    {
      .node_to_insert = {"@IPv4:ipv4@", NULL},
      .key = "192.168.1.1huhuhu",
      .expected_pattern = {"ipv4", "192.168.1.1", NULL}
    },
    {
      .node_to_insert = {"@IPv4:ipv4@", NULL},
      .key = "192.168.1huhuhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPv4:ipv4@", NULL},
      .key = "192.168.1.huhuhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPv4:ipv4@", NULL},
      .key = "192.168.1 huhuhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPv4:ipv4@", NULL},
      .key = "192.168.1. huhuhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPv4:ipv4@", NULL},
      .key = "v12345",
      .expected_pattern = {NULL}
    },
    /* test_ipv6_matches */
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "1:2:3:4:5:6:7 huhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "1:2:3:4:5:6:7.huhu",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "1:2:3:4:5:6:7:huhu",
      .expected_pattern = {NULL},
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "v12345",
      .expected_pattern = {NULL},
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789 huhuhu",
      .expected_pattern = {"ipv6", "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "abcd:ef01:2345:6789:abcd:ef01:2345:6789 huhuhu",
      .expected_pattern = {"ipv6", "abcd:ef01:2345:6789:abcd:ef01:2345:6789", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "0:0:0:0:0:0:0:0 huhuhu",
      .expected_pattern = {"ipv6", "0:0:0:0:0:0:0:0", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "2001:DB8::8:800:200C:417A huhuhu",
      .expected_pattern = {"ipv6", "2001:DB8::8:800:200C:417A", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "FF01::101 huhuhu",
      .expected_pattern = {"ipv6", "FF01::101", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::1 huhuhu",
      .expected_pattern = {"ipv6", "::1", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = ":: huhuhu",
      .expected_pattern = {"ipv6", "::", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "0:0:0:0:0:0:13.1.68.3 huhuhu",
      .expected_pattern = {"ipv6", "0:0:0:0:0:0:13.1.68.3", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::202.1.68.3 huhuhu",
      .expected_pattern = {"ipv6", "::202.1.68.3", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "2001:0DB8:0:CD30:: huhuhu",
      .expected_pattern = {"ipv6", "2001:0DB8:0:CD30::", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "2001:0DB8:0:CD30::huhuhu",
      .expected_pattern = {"ipv6", "2001:0DB8:0:CD30::", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::ffff:200.200.200.200huhuhu",
      .expected_pattern = {"ipv6", "::ffff:200.200.200.200", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "2001:0DB8:0:CD30::huhuhu",
      .expected_pattern = {"ipv6", "2001:0DB8:0:CD30::", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::ffff:200.200.200.200 :huhuhu",
      .expected_pattern = {"ipv6", "::ffff:200.200.200.200", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::ffff:200.200.200.200: huhuhu",
      .expected_pattern = {"ipv6", "::ffff:200.200.200.200", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::ffff:200.200.200.200. :huhuhu",
      .expected_pattern = {"ipv6", "::ffff:200.200.200.200", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::ffff:200.200.200.200.:huhuhu",
      .expected_pattern = {"ipv6", "::ffff:200.200.200.200", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::ffff:200.200.200.200.2:huhuhu",
      .expected_pattern = {"ipv6", "::ffff:200.200.200.200", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::0: huhuhu",
      .expected_pattern = {"ipv6", "::0", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "0:0:0:0:0:0:0:0: huhuhu",
      .expected_pattern = {"ipv6", "0:0:0:0:0:0:0:0", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::129.144.52.38: huhuhu",
      .expected_pattern = {"ipv6", "::129.144.52.38", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::ffff:129.144.52.38: huhuhu",
      .expected_pattern = {"ipv6", "::ffff:129.144.52.38", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789.huhuhu",
      .expected_pattern = {"ipv6", "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "abcd:ef01:2345:6789:abcd:ef01:2345:6789.huhuhu",
      .expected_pattern = {"ipv6", "abcd:ef01:2345:6789:abcd:ef01:2345:6789", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "0:0:0:0:0:0:0:0.huhuhu",
      .expected_pattern = {"ipv6", "0:0:0:0:0:0:0:0", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "2001:DB8::8:800:200C:417A.huhuhu",
      .expected_pattern = {"ipv6", "2001:DB8::8:800:200C:417A", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "FF01::101.huhuhu",
      .expected_pattern = {"ipv6", "FF01::101", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::1.huhuhu",
      .expected_pattern = {"ipv6", "::1", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::.huhuhu",
      .expected_pattern = {"ipv6", "::", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "0:0:0:0:0:0:13.1.68.3.huhuhu",
      .expected_pattern = {"ipv6", "0:0:0:0:0:0:13.1.68.3", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "::202.1.68.3.huhuhu",
      .expected_pattern = {"ipv6", "::202.1.68.3", NULL}
    },
    {
      .node_to_insert = {"@IPv6:ipv6@", NULL},
      .key = "2001:0DB8:0:CD30::.huhuhu",
      .expected_pattern = {"ipv6", "2001:0DB8:0:CD30::", NULL}
    },
    /* test_number_matches */
    {
      .node_to_insert = {"@NUMBER:number@", NULL},
      .key = "12345 hihihi",
      .expected_pattern = {"number", "12345", NULL}
    },
    {
      .node_to_insert = {"@NUMBER:number@", NULL},
      .key = "12345 hihihi",
      .expected_pattern = {"number", "12345", NULL}
    },
    {
      .node_to_insert = {"@NUMBER:number@", NULL},
      .key = "0xaf12345 hihihi",
      .expected_pattern = {"number", "0xaf12345", NULL}
    },
    {
      .node_to_insert = {"@NUMBER:number@", NULL},
      .key = "0xAF12345 hihihi",
      .expected_pattern = {"number", "0xAF12345", NULL}
    },
    {
      .node_to_insert = {"@NUMBER:number@", NULL},
      .key = "0x12345 hihihi",
      .expected_pattern = {"number", "0x12345", NULL}
    },
    {
      .node_to_insert = {"@NUMBER:number@", NULL},
      .key = "0XABCDEF12345ABCDEF hihihi",
      .expected_pattern = {"number", "0XABCDEF12345ABCDEF", NULL}
    },
    {
      .node_to_insert = {"@NUMBER:number@", NULL},
      .key = "-12345 hihihi",
      .expected_pattern = {"number", "-12345", NULL}
    },
    {
      .node_to_insert = {"@NUMBER:number@", NULL},
      .key = "v12345",
      .expected_pattern = {NULL}
    },
    /* test_qstring_matches */
    {
      .node_to_insert = {"@QSTRING:qstring:'@", NULL},
      .key = "'quoted string' hehehe",
      .expected_pattern = {"qstring", "quoted string", NULL}
    },
    {
      .node_to_insert = {"@QSTRING:qstring:'@", NULL},
      .key = "v12345",
      .expected_pattern = {NULL}
    },
    /* test_estring_matches */
    {
      .node_to_insert = {"ddd @ESTRING:estring::@",
        "dddd @ESTRING:estring::*@",
        "dddd2 @ESTRING:estring::*@ d",
        "zzz @ESTRING:test:gép@",
        NULL
      },
      .key = "ddd estring: hehehe",
      .expected_pattern = {"estring", "estring", NULL},
    },
    {
      .node_to_insert = {"ddd @ESTRING:estring::@",
        "dddd @ESTRING:estring::*@",
        "dddd2 @ESTRING:estring::*@ d",
        "zzz @ESTRING:test:gép@",
        NULL
      },
      .key = "ddd v12345",
      .expected_pattern = {NULL},
    },
    {
      .node_to_insert = {"ddd @ESTRING:estring::@",
        "dddd @ESTRING:estring::*@",
        "dddd2 @ESTRING:estring::*@ d",
        "zzz @ESTRING:test:gép@",
        NULL
      },
      .key = "dddd estring:* hehehe",
      .expected_pattern = {"estring", "estring", NULL},
    },
    {
      .node_to_insert = {"ddd @ESTRING:estring::@",
        "dddd @ESTRING:estring::*@",
        "dddd2 @ESTRING:estring::*@ d",
        "zzz @ESTRING:test:gép@",
        NULL
      },
      .key = "dddd estring:estring:* hehehe",
      .expected_pattern = {"estring", "estring:estring", NULL},
    },
    {
      .node_to_insert = {"ddd @ESTRING:estring::@",
        "dddd @ESTRING:estring::*@",
        "dddd2 @ESTRING:estring::*@ d",
        "zzz @ESTRING:test:gép@",
        NULL
      },
      .key = "dddd estring:estring::* hehehe",
      .expected_pattern = {"estring", "estring:estring:", NULL},
    },
    {
      .node_to_insert = {"ddd @ESTRING:estring::@",
        "dddd @ESTRING:estring::*@",
        "dddd2 @ESTRING:estring::*@ d",
        "zzz @ESTRING:test:gép@",
        NULL
      },
      .key = "dddd2 estring:estring::* d",
      .expected_pattern = {"estring", "estring:estring:", NULL}
    },
    {
      .node_to_insert = {"ddd @ESTRING:estring::@",
        "dddd @ESTRING:estring::*@",
        "dddd2 @ESTRING:estring::*@ d",
        "zzz @ESTRING:test:gép@",
        NULL
      },
      .key = "dddd2 estring:estring::* ",
      .expected_pattern = {NULL},
    },
    {
      .node_to_insert = {"ddd @ESTRING:estring::@",
        "dddd @ESTRING:estring::*@",
        "dddd2 @ESTRING:estring::*@ d",
        "zzz @ESTRING:test:gép@",
        NULL
      },
      .key = "dddd2 estring:estring::*",
      .expected_pattern = {NULL},
    },
    {
      .node_to_insert = {"ddd @ESTRING:estring::@",
        "dddd @ESTRING:estring::*@",
        "dddd2 @ESTRING:estring::*@ d",
        "zzz @ESTRING:test:gép@",
        NULL
      },
      .key = "dddd2 estring:estring:*",
      .expected_pattern = {NULL}
    },
    {
      .node_to_insert = {"ddd @ESTRING:estring::@",
        "dddd @ESTRING:estring::*@",
        "dddd2 @ESTRING:estring::*@ d",
        "zzz @ESTRING:test:gép@",
        NULL
      },
      .key = "dddd2 estring:estring",
      .expected_pattern = {NULL},
    },
    {
      .node_to_insert = {"ddd @ESTRING:estring::@",
        "dddd @ESTRING:estring::*@",
        "dddd2 @ESTRING:estring::*@ d",
        "zzz @ESTRING:test:gép@",
        NULL
      },
      .key = "dddd v12345",
      .expected_pattern = {NULL},
    },
    {
      .node_to_insert = {"ddd @ESTRING:estring::@",
        "dddd @ESTRING:estring::*@",
        "dddd2 @ESTRING:estring::*@ d",
        "zzz @ESTRING:test:gép@",
        NULL
      },
      .key = "zzz árvíztűrőtükörfúrógép",
      .expected_pattern = {"test", "árvíztűrőtükörfúró", NULL},
    },
    /* test_string_matches */
    {
      .node_to_insert = {"@STRING:string@", NULL},
      .key = "string hehehe",
      .expected_pattern = {"string", "string", NULL},
    },
    /* test_float_matches */
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "12345 hihihi",
      .expected_pattern = {"float", "12345", NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "12345hihihi",
      .expected_pattern = {"float", "12345", NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "12.345hihihi",
      .expected_pattern = {"float", "12.345", NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "12.345.hihihi",
      .expected_pattern = {"float", "12.345", NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "12.345.6hihihi",
      .expected_pattern = {"float", "12.345", NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "12345.hihihi",
      .expected_pattern = {"float", "12345.", NULL}
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "-12.345 hihihi",
      .expected_pattern = {"float", "-12.345", NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "-12.345e12 hihihi",
      .expected_pattern = {"float", "-12.345e12", NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "-12.345e-12 hihihi",
      .expected_pattern = {"float", "-12.345e-12", NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "12.345e12 hihihi",
      .expected_pattern = {"float", "12.345e12", NULL}
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "12.345e-12 hihihi",
      .expected_pattern = {"float", "12.345e-12", NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "-12.345E12 hihihi",
      .expected_pattern = {"float", "-12.345E12", NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "-12.345E-12 hihihi",
      .expected_pattern = {"float", "-12.345E-12", NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "12.345E12 hihihi",
      .expected_pattern = {"float", "12.345E12", NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "12.345E-12 hihihi",
      .expected_pattern = {"float", "12.345E-12", NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "v12345",
      .expected_pattern = {NULL},
    },
    {
      .node_to_insert = {"@FLOAT:float@", NULL},
      .key = "12345.hihihi",
      .expected_pattern = {"float", "12345.", NULL},
    },
    /* test_set_matches */
    {
      .node_to_insert = {"@SET:set:  @", NULL},
      .key = " aaa",
      .expected_pattern = {"set", " ", NULL},
    },
    {
      .node_to_insert = {"@SET:set:  @", NULL},
      .key = "  aaa",
      .expected_pattern = {"set", "  ", NULL},
    },
    /* test_mcaddr_matches */
    {
      .node_to_insert = {"@MACADDR:macaddr@", NULL},
      .key = "82:63:25:93:eb:51.iii",
      .expected_pattern = {"macaddr", "82:63:25:93:eb:51", NULL},
    },
    {
      .node_to_insert = {"@MACADDR:macaddr@", NULL},
      .key = "82:63:25:93:EB:51.iii",
      .expected_pattern = {"macaddr", "82:63:25:93:EB:51", NULL},
    },
    /* test_email_matches */
    {
      .node_to_insert = {"@EMAIL:email:[<]>@", NULL },
      .key = "blint@balabit.hu",
      .expected_pattern = {"email", "blint@balabit.hu", NULL},
    },
    {
      .node_to_insert = {"@EMAIL:email:[<]>@", NULL },
      .key = "<blint@balabit.hu>",
      .expected_pattern = {"email", "blint@balabit.hu", NULL},
    },
    {
      .node_to_insert = {"@EMAIL:email:[<]>@", NULL },
      .key = "[blint@balabit.hu]",
      .expected_pattern = {"email", "blint@balabit.hu", NULL},
    },
    /* test_hostname_matches */
    {
      .node_to_insert = {"@HOSTNAME:hostname@", NULL},
      .key = "www.example.org",
      .expected_pattern = {"hostname", "www.example.org", NULL},
    },
    {
      .node_to_insert = {"@HOSTNAME:hostname@", NULL},
      .key = "www.example.org. kkk",
      .expected_pattern = {"hostname", "www.example.org.", NULL},
    },
    /* test_lladdr_matches */
    {
      .node_to_insert = {"@LLADDR:lladdr6:6@", NULL},
      .key = "83:63:25:93:eb:51:aa:bb.iii",
      .expected_pattern = {"lladdr6", "83:63:25:93:eb:51", NULL},
    },
    {
      .node_to_insert = {"@LLADDR:lladdr6:6@", NULL},
      .key = "83:63:25:93:EB:51:aa:bb.iii",
      .expected_pattern = {"lladdr6", "83:63:25:93:EB:51", NULL},
    },
    /* test_pcre_matches */
    {
      .node_to_insert = {"jjj @PCRE:regexp:[abc]+@", "jjjj @PCRE:regexp:[abc]+@d foobar", NULL},
      .key = "jjj abcabcd",
      .expected_pattern = {"regexp", "abcabc", NULL},
    },
    {
      .node_to_insert = {"jjj @PCRE:regexp:[abc]+@", "jjjj @PCRE:regexp:[abc]+@d foobar", NULL},
      .key = "jjjj abcabcd foobar",
      .expected_pattern = {"regexp", "abcabc", NULL},
    },
    {
      .node_to_insert = {"@PCRE:regexp:(foo|bar)@", NULL},
      .key = "foo",
      .expected_pattern = {"regexp", "foo", NULL},
    },
    {
      .node_to_insert = {"@PCRE:regexp:(?:foo|bar)@", NULL},
      .key = "foo",
      .expected_pattern = {"regexp", "foo", NULL},
    },
    /* test_nlstring_matches */
    {
      .node_to_insert = {"@NLSTRING:nlstring@\n", NULL},
      .key = "foobar\r\nbaz",
      .expected_pattern = {"nlstring", "foobar", NULL},
    },
    {
      .node_to_insert = {"@NLSTRING:nlstring@\n", NULL},
      .key = "foobar\nbaz",
      .expected_pattern = {"nlstring", "foobar", NULL},
    },
    {
      .node_to_insert = {"@NLSTRING:nlstring@\n", NULL},
      .key = "\nbaz",
      .expected_pattern = {"nlstring", "", NULL},
    },
    {
      .node_to_insert = {"@NLSTRING:nlstring@\n", NULL},
      .key = "\r\nbaz",
      .expected_pattern = {"nlstring", "", NULL},
    }
  };
  return cr_make_param_array(RadixTestParam, parser_params, G_N_ELEMENTS(parser_params));
}

ParameterizedTest(RadixTestParam *param, dbparser, test_radix_search_matches, .init = test_setup, .fini = test_teardown)
{
  RNode *root = r_new_node("", NULL);

  for (int i=0; param->node_to_insert[i]; i++)
    insert_node(root, param->node_to_insert[i]);

  test_search_matches(root, param->key, param->expected_pattern);
  r_free_node(root, NULL);
}

Test(dbparser, test_radix_zorp_log, .init = test_setup, .fini = test_teardown)
{
  RNode *root = r_new_node("", NULL);

  /* these are conflicting logs */
  insert_node_with_value(root,
                         "core.error(2): (svc/@STRING:service:._@:@NUMBER:instance_id@/plug): Connection to remote end failed; local='AF_INET(@IPv4:local_ip@:@NUMBER:local_port@)', remote='AF_INET(@IPv4:remote_ip@:@NUMBER:remote_port@)', error=@QSTRING:errormsg:'@",
                         "ZORP");
  insert_node_with_value(root,
                         "core.error(2): (svc/@STRING:service:._@:@NUMBER:instance_id@/plug): Connection to remote end failed; local=@QSTRING:p:'@, remote=@QSTRING:p:'@, error=@QSTRING:p:'@",
                         "ZORP1");
  insert_node_with_value(root,
                         "Deny@QSTRING:FIREWALL.DENY_PROTO: @src@QSTRING:FIREWALL.DENY_O_INT: :@@IPv4:FIREWALL.DENY_SRCIP@/@NUMBER:FIREWALL.DENY_SRCPORT@ dst",
                         "CISCO");
  insert_node_with_value(root, "@NUMBER:Seq@, @ESTRING:DateTime:,@@ESTRING:Severity:,@@ESTRING:Comp:,@", "3com");

  test_search_value(root,
                    "core.error(2): (svc/intra.servers.alef_SSH_dmz.zajin:111/plug): Connection to remote end failed; local='AF_INET(172.16.0.1:56867)', remote='AF_INET(172.18.0.1:22)', error='No route to host'PAS",
                    "ZORP");
  test_search_value(root,
                    "Deny udp src OUTSIDE:10.0.0.0/1234 dst INSIDE:192.168.0.0/5678 by access-group \"OUTSIDE\" [0xb74026ad, 0x0]",
                    "CISCO");

  const gchar *search_pattern[] = {"Seq", "1", "DateTime", "2006-08-22 16:31:39", "Severity", "INFO", "Comp", "BLK", NULL};
  test_search_matches(root, "1, 2006-08-22 16:31:39,INFO,BLK,", search_pattern);

  r_free_node(root, NULL);
}
