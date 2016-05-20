/*
 * Copyright (c) 2008-2015 Balabit
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

gboolean fail = FALSE;
gboolean verbose = FALSE;

void r_print_node(RNode *node, int depth);

void
r_print_pnode(RNode *node, int depth)
{
  int i;
  for (i = 0; i < depth; i++)
    printf(" ");

  printf("%dPNODE: %d('%s') => '%s'\n", depth, node->parser->type, log_msg_get_value_name(node->parser->handle, NULL), (char *)node->value);

  for (i = 0; i < node->num_children; i++)
    r_print_node(node->children[i], depth + 1);

  for (i = 0; i < node->num_pchildren; i++)
    r_print_pnode(node->pchildren[i], depth + 1);
}

void
r_print_node(RNode *node, int depth)
{
  int i;
  for (i = 0; i < depth; i++)
    printf(" ");

  printf("%dNODE: '%s' => '%s'\n", depth, node->key, (char *)node->value);

  for (i = 0; i < node->num_children; i++)
    r_print_node(node->children[i], depth + 1);

  for (i = 0; i < node->num_pchildren; i++)
    r_print_pnode(node->pchildren[i], depth + 1);
}

void
insert_node_with_value(RNode *root, gchar *key, gpointer value)
{
  gchar *dup;

  /* NOTE: we need to duplicate the key as r_insert_node modifies its input
   * and it might be a read-only string literal */

  dup = g_strdup(key);
  r_insert_node(root, dup, value ? : key, NULL);
  g_free(dup);
}

void
insert_node(RNode *root, gchar *key)
{
  insert_node_with_value(root, key, NULL);
}

void
test_search_value(RNode *root, gchar *key, gchar *expected_value)
{
  RNode *ret = r_find_node(root, key, strlen(key), NULL);

  if (ret && expected_value)
    {
      if (strcmp(ret->value, expected_value) != 0)
        {
          printf("FAIL: returned value does not match expected: '%s' <> '%s'\n", (gchar *) ret->value, expected_value);
          fail = TRUE;
        }
      else if (verbose)
        {
          printf("PASS: found expected: '%s' => '%s'\n", key, (gchar *) ret->value);
        }
    }
  else if (ret && !expected_value)
    {
      printf("FAIL: found unexpected: '%s' => '%s'\n", key, (gchar *) ret->value);
      fail = TRUE;
    }
  else if (expected_value)
    {
      printf("FAIL: not found while expected: '%s' => none\n", key);
      fail = TRUE;
    }
}

void
test_search(RNode *root, gchar *key, gboolean expect)
{
  test_search_value(root, key, expect ? key : NULL);
}

void
test_search_matches(RNode *root, gchar *key, gchar *name1, ...)
{
  RNode *ret;
  va_list args;
  GArray *matches = g_array_new(FALSE, TRUE, sizeof(RParserMatch));
  RParserMatch *match;
  const gchar *match_name;

  g_array_set_size(matches, 1);
  va_start(args, name1);

  ret = r_find_node(root, key, strlen(key), matches);
  if (ret && !name1)
    {
      printf("FAIL: found unexpected: '%s' => '%s' matches: ", key, (gchar *) ret->value);
      for (gsize i = 0; i < matches->len; i++)
        {
          match = &g_array_index(matches, RParserMatch, i);
          match_name = log_msg_get_value_name(match->handle, NULL);
          if (match_name)
            {
              if (!match->match)
                printf("'%s' => '%.*s'", match_name, match->len, &key[match->ofs]);
              else
                printf("'%s' => '%s'", match_name, match->match);
            }
        }
      printf("\n");
      fail = TRUE;
    }
  else if (ret && name1)
    {
      gsize i = 1;
      gchar *name, *value;

      name = name1;
      value = va_arg(args, gchar *);
      while (name)
        {
          if (i >= matches->len)
            {
              printf("FAIL: not enough matches: '%s' => expecting %" G_GSIZE_FORMAT ". match, value %s, "
                     " total %u\n",
                     key, i, value, matches->len);
              fail = TRUE;
              goto out;
            }
          match = &g_array_index(matches, RParserMatch, i);
          match_name = log_msg_get_value_name(match->handle, NULL);

          if (strcmp(match_name, name) != 0)
            {
              printf("FAIL: name does not match: '%s' => expecting %" G_GSIZE_FORMAT ". match, "
                     "name %s != %s\n",
                     key, i, name, match_name);
              fail = TRUE;
            }

          if (!match->match)
            {
              if (strncmp(&key[match->ofs], value, match->len) != 0 || strlen(value) != match->len)
                {
                  printf("FAIL: value does not match: '%s' => expecting %" G_GSIZE_FORMAT ". match, "
                         "value '%s' != '%.*s', total %d\n",
                         key, i, value, match->len, &key[match->ofs], matches->len);
                  fail = TRUE;
                }
            }
          else
            {
              if (strcmp(match->match, value) != 0)
                {
                  printf("FAIL: value does not match: '%s' => expecting %" G_GSIZE_FORMAT ". match, "
                         "value '%s' != '%s', total %d\n",
                         key, i, value, match->match, matches->len);
                  fail = TRUE;
                }
            }
          name = va_arg(args, gchar *);
          if (name)
            value = va_arg(args, gchar *);
          i++;
        }
      if (verbose)
        printf("PASS: all values match: '%s'\n", key);
    }
  else if (!ret && !name1)
    {
      if (verbose)
        printf("PASS: nothing was matched as expected: '%s'\n", key);
    }
  else
    {
      printf("FAIL: not found while expected: '%s' => none\n", key);
      fail = TRUE;
    }

 out:
  va_end(args);

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

void
test_literals(void)
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

void
test_parsers(void)
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

  printf("We excpect an error message\n");
  insert_node(root, "xxx@ESTRING@");
  printf("We excpect an error message\n");
  insert_node(root, "xxx@QSTRING@");
  insert_node(root, "xxx@STRING@");
  insert_node(root, "xxx@ANYSTRING@");

  printf("We excpect an error message\n");
  insert_node(root, "xxx@ESTRING@x");
  printf("We excpect an error message\n");
  insert_node(root, "xxx@QSTRING@x");
  insert_node(root, "xxx@STRING@x");
  insert_node(root, "xxx@ANYSTRING@x");
  printf("We excpect an error message\n");
  insert_node(root, "AAA@NUMBER:invalid=@AAA");
  printf("We excpect an error message\n");
  insert_node(root, "AAA@SET@AAA");
  printf("We excpect an error message\n");
  insert_node(root, "AAA@SET:set@AAA");
  insert_node(root, "AAA@MACADDR@AAA");
  insert_node(root, "newline@NUMBER@\n2ndline\n");

  printf("We excpect an error message\n");
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

  /* FIXME: this one fails, because the shorter match is returned. The
   * current radix implementation does not ensure the longest match when the
   * radix tree is split on a parser node. */

#if 0
  test_search_value(root, "a15555aaa", "a@NUMBER:szamx@aaa");
#endif
  test_search_value(root, "@a", "@@a");
  test_search_value(root, "@", "@@");
  test_search_value(root, "@@", "@@@@");

  r_free_node(root, NULL);
}

void
test_ip_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "@IPvANY:ip@");

  test_search_matches(root, "192.168.1.1 huhuhu",
                      "ip", "192.168.1.1",
                      NULL);

  test_search_matches(root, "192.168.1.1 huhuhu",
                      "ip", "192.168.1.1",
                      NULL);

  test_search_matches(root, "192.168.1.1. huhuhu",
                      "ip", "192.168.1.1",
                      NULL);

  test_search_matches(root, "192.168.1huhuhu", NULL);
  test_search_matches(root, "192.168.1.huhuhu", NULL);
  test_search_matches(root, "192.168.1 huhuhu", NULL);
  test_search_matches(root, "192.168.1. huhuhu", NULL);
  test_search_matches(root, "192.168.1.1huhuhu",
                      "ip", "192.168.1.1",
                      NULL);

  test_search_matches(root, "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789 huhuhu",
                      "ip", "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789", NULL);

  test_search_matches(root, "abcd:ef01:2345:6789:abcd:ef01:2345:6789 huhuhu",
                      "ip", "abcd:ef01:2345:6789:abcd:ef01:2345:6789", NULL);

  test_search_matches(root, ":: huhuhu",
                      "ip", "::", NULL);

  test_search_matches(root, "0:0:0:0:0:0:13.1.68.3 huhuhu",
                      "ip", "0:0:0:0:0:0:13.1.68.3", NULL);

  test_search_matches(root, "::202.1.68.3 huhuhu",
                      "ip", "::202.1.68.3", NULL);

  test_search_matches(root, "2001:0DB8:0:CD30:: huhuhu",
                      "ip", "2001:0DB8:0:CD30::", NULL);

  test_search_matches(root, "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789.huhuhu",
                      "ip", "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789", NULL);

  test_search_matches(root, "abcd:ef01:2345:6789:abcd:ef01:2345:6789.huhuhu",
                      "ip", "abcd:ef01:2345:6789:abcd:ef01:2345:6789", NULL);

  test_search_matches(root, "::.huhuhu",
                      "ip", "::", NULL);

  test_search_matches(root, "0:0:0:0:0:0:13.1.68.3.huhuhu",
                      "ip", "0:0:0:0:0:0:13.1.68.3", NULL);

  test_search_matches(root, "::202.1.68.3.huhuhu",
                      "ip", "::202.1.68.3", NULL);

  test_search_matches(root, "2001:0DB8:0:CD30::.huhuhu",
                      "ip", "2001:0DB8:0:CD30::", NULL);

  test_search_matches(root, "1:2:3:4:5:6:7:8.huhuhu",
                      "ip", "1:2:3:4:5:6:7:8", NULL);

  test_search_matches(root, "1:2:3:4:5:6:7:8 huhuhu",
                      "ip", "1:2:3:4:5:6:7:8", NULL);

  test_search_matches(root, "1:2:3:4:5:6:7:8:huhuhu",
                      "ip", "1:2:3:4:5:6:7:8", NULL);

  test_search_matches(root, "1:2:3:4:5:6:7 huhu", NULL);
  test_search_matches(root, "1:2:3:4:5:6:7.huhu", NULL);
  test_search_matches(root, "1:2:3:4:5:6:7:huhu", NULL);
  test_search_matches(root, "1:2:3:4:5:6:77777:8 huhu", NULL);
  test_search_matches(root, "1:2:3:4:5:6:1.2.333.4 huhu", NULL);

  test_search_matches(root, "v12345", NULL);

  r_free_node(root, NULL);
}

void
test_ipv4_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "@IPv4:ipv4@");

  test_search_matches(root, "192.168.1.1 huhuhu",
                      "ipv4", "192.168.1.1",
                      NULL);

  test_search_matches(root, "192.168.1.1. huhuhu",
                      "ipv4", "192.168.1.1",
                      NULL);

  test_search_matches(root, "192.168.1.1.huhuhu",
                      "ipv4", "192.168.1.1",
                      NULL);

  test_search_matches(root, "192.168.1.1.. huhuhu",
                      "ipv4", "192.168.1.1",
                      NULL);

  test_search_matches(root, "192.168.1.1.2 huhuhu",
                      "ipv4", "192.168.1.1",
                      NULL);

  test_search_matches(root, "192.168.1.1..huhuhu",
                      "ipv4", "192.168.1.1",
                      NULL);

  test_search_matches(root, "192.168.1.1huhuhu",
                      "ipv4", "192.168.1.1",
                      NULL);

  test_search_matches(root, "192.168.1huhuhu", NULL);
  test_search_matches(root, "192.168.1.huhuhu", NULL);
  test_search_matches(root, "192.168.1 huhuhu", NULL);
  test_search_matches(root, "192.168.1. huhuhu", NULL);

  test_search_matches(root, "v12345", NULL);

  r_free_node(root, NULL);
}

void
test_ipv6_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "@IPv6:ipv6@");

  test_search_matches(root, "1:2:3:4:5:6:7 huhu", NULL);
  test_search_matches(root, "1:2:3:4:5:6:7.huhu", NULL);
  test_search_matches(root, "1:2:3:4:5:6:7:huhu", NULL);

  test_search_matches(root, "v12345", NULL);

  test_search_matches(root, "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789 huhuhu",
                      "ipv6", "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789", NULL);

  test_search_matches(root, "abcd:ef01:2345:6789:abcd:ef01:2345:6789 huhuhu",
                      "ipv6", "abcd:ef01:2345:6789:abcd:ef01:2345:6789", NULL);

  test_search_matches(root, "0:0:0:0:0:0:0:0 huhuhu",
                      "ipv6", "0:0:0:0:0:0:0:0", NULL);

  test_search_matches(root, "2001:DB8::8:800:200C:417A huhuhu",
                      "ipv6", "2001:DB8::8:800:200C:417A", NULL);

  test_search_matches(root, "FF01::101 huhuhu",
                      "ipv6", "FF01::101", NULL);

  test_search_matches(root, "::1 huhuhu",
                      "ipv6", "::1", NULL);

  test_search_matches(root, ":: huhuhu",
                      "ipv6", "::", NULL);

  test_search_matches(root, "0:0:0:0:0:0:13.1.68.3 huhuhu",
                      "ipv6", "0:0:0:0:0:0:13.1.68.3", NULL);

  test_search_matches(root, "::202.1.68.3 huhuhu",
                      "ipv6", "::202.1.68.3", NULL);

  test_search_matches(root, "2001:0DB8:0:CD30:: huhuhu",
                      "ipv6", "2001:0DB8:0:CD30::", NULL);

  test_search_matches(root, "2001:0DB8:0:CD30::huhuhu",
                      "ipv6", "2001:0DB8:0:CD30::", NULL);

  test_search_matches(root, "::ffff:200.200.200.200huhuhu",
                      "ipv6", "::ffff:200.200.200.200", NULL);

  test_search_matches(root, "2001:0DB8:0:CD30::huhuhu",
                      "ipv6", "2001:0DB8:0:CD30::", NULL);

  test_search_matches(root, "::ffff:200.200.200.200 :huhuhu",
                      "ipv6", "::ffff:200.200.200.200", NULL);

  test_search_matches(root, "::ffff:200.200.200.200: huhuhu",
                      "ipv6", "::ffff:200.200.200.200", NULL);

  test_search_matches(root, "::ffff:200.200.200.200. :huhuhu",
                      "ipv6", "::ffff:200.200.200.200", NULL);

  test_search_matches(root, "::ffff:200.200.200.200.:huhuhu",
                      "ipv6", "::ffff:200.200.200.200", NULL);

  test_search_matches(root, "::ffff:200.200.200.200.2:huhuhu",
                      "ipv6", "::ffff:200.200.200.200", NULL);

  test_search_matches(root, "::0: huhuhu",
                      "ipv6", "::0", NULL);

  test_search_matches(root, "0:0:0:0:0:0:0:0: huhuhu",
                      "ipv6", "0:0:0:0:0:0:0:0", NULL);

  test_search_matches(root, "::129.144.52.38: huhuhu",
                      "ipv6", "::129.144.52.38", NULL);

  test_search_matches(root, "::ffff:129.144.52.38: huhuhu",
                      "ipv6", "::ffff:129.144.52.38", NULL);

  test_search_matches(root, "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789.huhuhu",
                      "ipv6", "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789", NULL);

  test_search_matches(root, "abcd:ef01:2345:6789:abcd:ef01:2345:6789.huhuhu",
                      "ipv6", "abcd:ef01:2345:6789:abcd:ef01:2345:6789", NULL);

  test_search_matches(root, "0:0:0:0:0:0:0:0.huhuhu",
                      "ipv6", "0:0:0:0:0:0:0:0", NULL);

  test_search_matches(root, "2001:DB8::8:800:200C:417A.huhuhu",
                      "ipv6", "2001:DB8::8:800:200C:417A", NULL);

  test_search_matches(root, "FF01::101.huhuhu",
                      "ipv6", "FF01::101", NULL);

  test_search_matches(root, "::1.huhuhu",
                      "ipv6", "::1", NULL);

  test_search_matches(root, "::.huhuhu",
                      "ipv6", "::", NULL);

  test_search_matches(root, "0:0:0:0:0:0:13.1.68.3.huhuhu",
                      "ipv6", "0:0:0:0:0:0:13.1.68.3", NULL);

  test_search_matches(root, "::202.1.68.3.huhuhu",
                      "ipv6", "::202.1.68.3", NULL);

  test_search_matches(root, "2001:0DB8:0:CD30::.huhuhu",
                      "ipv6", "2001:0DB8:0:CD30::", NULL);

  r_free_node(root, NULL);
}

void
test_number_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "@NUMBER:number@");

  test_search_matches(root, "12345 hihihi",
                      "number", "12345",
                      NULL);

  test_search_matches(root, "12345 hihihi",
                      "number", "12345",
                      NULL);

  test_search_matches(root, "0xaf12345 hihihi",
                      "number", "0xaf12345",
                      NULL);

  test_search_matches(root, "0xAF12345 hihihi",
                      "number", "0xAF12345",
                      NULL);

  test_search_matches(root, "0x12345 hihihi",
                      "number", "0x12345",
                      NULL);

  test_search_matches(root, "0XABCDEF12345ABCDEF hihihi",
                      "number", "0XABCDEF12345ABCDEF",
                      NULL);

  test_search_matches(root, "-12345 hihihi",
                      "number", "-12345",
                      NULL);

  test_search_matches(root, "v12345", NULL);

  r_free_node(root, NULL);
}

void
test_qstring_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "@QSTRING:qstring:'@");

  test_search_matches(root, "'quoted string' hehehe",
                      "qstring", "quoted string",
                      NULL);

  test_search_matches(root, "v12345", NULL);

  r_free_node(root, NULL);
}

void
test_estring_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "ddd @ESTRING:estring::@");
  insert_node(root, "dddd @ESTRING:estring::*@");
  insert_node(root, "dddd2 @ESTRING:estring::*@ d");
  insert_node(root, "zzz @ESTRING:test:gép@");

  test_search_matches(root, "ddd estring: hehehe",
                      "estring", "estring",
                      NULL);

  test_search_matches(root, "ddd v12345", NULL);

  test_search_matches(root, "dddd estring:* hehehe",
                      "estring", "estring",
                      NULL);

  test_search_matches(root, "dddd estring:estring:* hehehe",
                      "estring", "estring:estring",
                      NULL);

  test_search_matches(root, "dddd estring:estring::* hehehe",
                      "estring", "estring:estring:",
                      NULL);

  test_search_matches(root, "dddd2 estring:estring::* d",
                      "estring", "estring:estring:",
                      NULL);

  test_search_matches(root, "dddd2 estring:estring::* ", NULL);
  test_search_matches(root, "dddd2 estring:estring::*", NULL);
  test_search_matches(root, "dddd2 estring:estring:*", NULL);
  test_search_matches(root, "dddd2 estring:estring", NULL);

  test_search_matches(root, "dddd v12345", NULL);

  test_search_matches(root, "zzz árvíztűrőtükörfúrógép", "test", "árvíztűrőtükörfúró", NULL);

  r_free_node(root, NULL);
}

void
test_string_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "@STRING:string@");

  test_search_matches(root, "string hehehe",
                      "string", "string",
                      NULL);

  r_free_node(root, NULL);
}

void
test_float_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "@FLOAT:float@");

  test_search_matches(root, "12345 hihihi",
                      "float", "12345", NULL);

  test_search_matches(root, "12345hihihi",
                      "float", "12345", NULL);

  test_search_matches(root, "12.345hihihi",
                      "float", "12.345", NULL);

  test_search_matches(root, "12.345.hihihi",
                      "float", "12.345", NULL);

  test_search_matches(root, "12.345.6hihihi",
                      "float", "12.345", NULL);

  test_search_matches(root, "12345.hihihi",
                      "float", "12345.", NULL);

  test_search_matches(root, "-12.345 hihihi",
                      "float", "-12.345", NULL);

  test_search_matches(root, "-12.345e12 hihihi",
                      "float", "-12.345e12", NULL);

  test_search_matches(root, "-12.345e-12 hihihi",
                      "float", "-12.345e-12", NULL);

  test_search_matches(root, "12.345e12 hihihi",
                      "float", "12.345e12", NULL);

  test_search_matches(root, "12.345e-12 hihihi",
                      "float", "12.345e-12", NULL);

  test_search_matches(root, "-12.345E12 hihihi",
                      "float", "-12.345E12", NULL);

  test_search_matches(root, "-12.345E-12 hihihi",
                      "float", "-12.345E-12", NULL);

  test_search_matches(root, "12.345E12 hihihi",
                      "float", "12.345E12", NULL);

  test_search_matches(root, "12.345E-12 hihihi",
                      "float", "12.345E-12", NULL);

  test_search_matches(root, "v12345", NULL);
  test_search_matches(root, "12345.hihihi","float", "12345.", NULL);

  r_free_node(root, NULL);
}

void
test_set_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "@SET:set:  @");

  test_search_matches(root, " aaa", "set", " ", NULL);
  test_search_matches(root, "  aaa", "set", "  ", NULL);

  r_free_node(root, NULL);
}

void
test_macaddr_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "@MACADDR:macaddr@");

  test_search_matches(root, "82:63:25:93:eb:51.iii", "macaddr", "82:63:25:93:eb:51", NULL);
  test_search_matches(root, "82:63:25:93:EB:51.iii", "macaddr", "82:63:25:93:EB:51", NULL);

  r_free_node(root, NULL);
}

void
test_email_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "@EMAIL:email:[<]>@");

  test_search_matches(root, "blint@balabit.hu","email", "blint@balabit.hu", NULL);
  test_search_matches(root, "<blint@balabit.hu>","email", "blint@balabit.hu", NULL);
  test_search_matches(root, "[blint@balabit.hu]","email", "blint@balabit.hu", NULL);

  r_free_node(root, NULL);
}

void
test_hostname_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "@HOSTNAME:hostname@");

  test_search_matches(root, "www.example.org", "hostname", "www.example.org", NULL);
  test_search_matches(root, "www.example.org. kkk", "hostname", "www.example.org.", NULL);

  r_free_node(root, NULL);
}

void
test_lladdr_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "@LLADDR:lladdr6:6@");

  test_search_matches(root, "83:63:25:93:eb:51:aa:bb.iii", "lladdr6", "83:63:25:93:eb:51", NULL);
  test_search_matches(root, "83:63:25:93:EB:51:aa:bb.iii", "lladdr6", "83:63:25:93:EB:51", NULL);

  r_free_node(root, NULL);
}

void
test_pcre_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "jjj @PCRE:regexp:[abc]+@");
  insert_node(root, "jjjj @PCRE:regexp:[abc]+@d foobar");

  test_search_matches(root, "jjj abcabcd", "regexp", "abcabc", NULL);
  test_search_matches(root, "jjjj abcabcd foobar", "regexp", "abcabc", NULL);

  r_free_node(root, NULL);
}

void
test_nlstring_matches(void)
{
  RNode *root = r_new_node("", NULL);
  insert_node(root, "@NLSTRING:nlstring@\n");

  test_search_matches(root, "foobar\r\nbaz", "nlstring", "foobar", NULL);
  test_search_matches(root, "foobar\nbaz", "nlstring", "foobar", NULL);
  test_search_matches(root, "\nbaz", "nlstring", "", NULL);
  test_search_matches(root, "\r\nbaz", "nlstring", "", NULL);

  r_free_node(root, NULL);
}

void
test_zorp_logs(void)
{
  RNode *root = r_new_node("", NULL);

  /* these are conflicting logs */
  insert_node_with_value(root, "core.error(2): (svc/@STRING:service:._@:@NUMBER:instance_id@/plug): Connection to remote end failed; local='AF_INET(@IPv4:local_ip@:@NUMBER:local_port@)', remote='AF_INET(@IPv4:remote_ip@:@NUMBER:remote_port@)', error=@QSTRING:errormsg:'@", "ZORP");
  insert_node_with_value(root, "core.error(2): (svc/@STRING:service:._@:@NUMBER:instance_id@/plug): Connection to remote end failed; local=@QSTRING:p:'@, remote=@QSTRING:p:'@, error=@QSTRING:p:'@", "ZORP1");
  insert_node_with_value(root, "Deny@QSTRING:FIREWALL.DENY_PROTO: @src@QSTRING:FIREWALL.DENY_O_INT: :@@IPv4:FIREWALL.DENY_SRCIP@/@NUMBER:FIREWALL.DENY_SRCPORT@ dst", "CISCO");
  insert_node_with_value(root, "@NUMBER:Seq@, @ESTRING:DateTime:,@@ESTRING:Severity:,@@ESTRING:Comp:,@", "3com");

  test_search_value(root, "core.error(2): (svc/intra.servers.alef_SSH_dmz.zajin:111/plug): Connection to remote end failed; local='AF_INET(172.16.0.1:56867)', remote='AF_INET(172.18.0.1:22)', error='No route to host'PAS", "ZORP");
  test_search_value(root, "Deny udp src OUTSIDE:10.0.0.0/1234 dst INSIDE:192.168.0.0/5678 by access-group \"OUTSIDE\" [0xb74026ad, 0x0]", "CISCO");
  test_search_matches(root, "1, 2006-08-22 16:31:39,INFO,BLK,", "Seq", "1", "DateTime", "2006-08-22 16:31:39", "Severity", "INFO", "Comp", "BLK", NULL);

  r_free_node(root, NULL);

}


int
main(int argc, char *argv[])
{
  app_startup();

  if (argc > 1)
    verbose = TRUE;

  msg_init(TRUE);

  test_literals();
  test_parsers();

  test_ip_matches();
  test_ipv4_matches();
  test_ipv6_matches();
  test_number_matches();
  test_qstring_matches();
  test_estring_matches();
  test_string_matches();
  test_float_matches();
  test_set_matches();
  test_macaddr_matches();
  test_email_matches();
  test_hostname_matches();
  test_lladdr_matches();
  test_pcre_matches();
  test_nlstring_matches();

  test_zorp_logs();

  app_shutdown();
  return  (fail ? 1 : 0);
}
