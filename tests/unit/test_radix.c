
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

  printf("%dPNODE: %d('%s') => '%s'\n", depth, node->parser->type, node->parser->name, (char *)node->value);

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
insert_node(RNode *root, gchar *key)
{
  gchar *dup;

  /* NOTE: we need to duplicate the key as r_insert_node modifies its input
   * and it might be a read-only string literal */

  dup = g_strdup(key);
  r_insert_node(root, dup, key, TRUE, NULL);
  g_free(dup);
}

void
test_search_value(RNode *root, gchar *key, gchar *expected_value)
{
  RNode *ret = r_find_node(root, key, key, strlen(key), NULL, NULL);

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
  GArray *matches = g_array_new(FALSE, TRUE, sizeof(LogMessageMatch));
  GPtrArray *match_names = g_ptr_array_new();
  LogMessageMatch *match;
  gchar *match_name;
  gint i;

  g_array_set_size(matches, 1);
  g_ptr_array_set_size(match_names, 1);
  va_start(args, name1);

  ret = r_find_node(root, key, key, strlen(key), matches, match_names);
  if (ret && !name1)
    {
      printf("FAIL: found unexpected: '%s' => '%s'\n", key, (gchar *) ret->value);
      fail = TRUE;
    }
  else if (ret && name1)
    {
      gint i = 1;
      gchar *name, *value;

      name = name1;
      value = va_arg(args, gchar *);
      while (name)
        {
          if (i >= matches->len)
            {
              printf("FAIL: not enough matches: '%s' => expecting %d. match, value %s, total %d\n", key, i, value, matches->len);
              fail = TRUE;
              goto out;
            }
          match = &g_array_index(matches, LogMessageMatch, i);
          match_name = g_ptr_array_index(match_names, i);

          if (strcmp(match_name, name) != 0)
            {
              printf("FAIL: name does not match: '%s' => expecting %d. match, name %s != %s\n", key, i, name, match_name);
              fail = TRUE;
            }

          if (match->flags & LMM_REF_MATCH)
            {
              if (strncmp(&key[match->ofs], value, match->len) != 0)
                {
                  printf("FAIL: value does not match: '%s' => expecting %d. match, value %s != %.*s, total %d\n", key, i, value, match->len, &key[match->ofs], matches->len);
                  fail = TRUE;
                }
            }
          else
            {
              if (strcmp(match->match, value) != 0)
                {
                  printf("FAIL: value does not match: '%s' => expecting %d. match, value %s != %s, total %d\n", key, i, value, match->match, matches->len);
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
  else
    {
      printf("FAIL: not found while expected: '%s' => none\n", key);
      fail = TRUE;
    }

 out:
  va_end(args);

  for (i = 0; i < matches->len; i++)
    {
      match_name = g_ptr_array_index(match_names, i);

      match = &g_array_index(matches, LogMessageMatch, i);
      if ((match->flags & LMM_REF_MATCH) == 0)
        {
          g_free(match->match);
        }
      g_free(match_name);
    }
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

  test_search_value(root, "a@", NULL);
  test_search_value(root, "a@NUMBER@aa@@", "a@@NUMBER@@aa@@@@");
  test_search_value(root, "a@a", NULL);
  test_search_value(root, "a@ab", "a@@ab");
  test_search_value(root, "a@a@", "a@@a@@");
  test_search_value(root, "a@ax", NULL);

  test_search_value(root, "a@15555", "a@@@NUMBER:szam0@");

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
test_matches(void)
{
  RNode *root = r_new_node("", NULL);

  insert_node(root, "aaa @NUMBER:number@");
  insert_node(root, "bbb @IPvANY:ip@");
  insert_node(root, "bbb4 @IPv4:ipv4@");
  insert_node(root, "bbb6 @IPv6:ipv6@");
  insert_node(root, "ccc @QSTRING:qstring:'@");
  insert_node(root, "ddd @ESTRING:estring::@");
  insert_node(root, "eee @STRING:string@");
  insert_node(root, "fff @FLOAT:float@");

  test_search_matches(root, "aaa 12345 hihihi",
                      "number", "12345",
                      NULL);

  test_search_matches(root, "bbb 192.168.1.1 huhuhu",
                      "ip", "192.168.1.1",
                      NULL);

  test_search_matches(root, "aaa 12345 hihihi",
                      "number", "12345",
                      NULL);

  test_search_matches(root, "aaa 0xaf12345 hihihi",
                      "number", "0xaf12345",
                      NULL);

  test_search_matches(root, "aaa 0xAF12345 hihihi",
                      "number", "0xAF12345",
                      NULL);

  test_search_matches(root, "aaa 0x12345 hihihi",
                      "number", "0x12345",
                      NULL);

  test_search_matches(root, "aaa 0XABCDEF12345ABCDEF hihihi",
                      "number", "0XABCDEF12345ABCDEF",
                      NULL);

  test_search_matches(root, "bbb 192.168.1.1 huhuhu",
                      "ip", "192.168.1.1",
                      NULL);

  test_search_matches(root, "bbb4 192.168.1.1 huhuhu",
                      "ipv4", "192.168.1.1",
                      NULL);

  test_search_matches(root, "bbb 192.168.1.1huhuhu",
                      "ip", "192.168.1.1",
                      NULL);

  test_search_matches(root, "bbb4 192.168.1.1huhuhu",
                      "ipv4", "192.168.1.1",
                      NULL);

  test_search_matches(root, "bbb6 ABCD:EF01:2345:6789:ABCD:EF01:2345:6789 huhuhu",
                      "ipv6", "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789", NULL);

  test_search_matches(root, "bbb6 abcd:ef01:2345:6789:abcd:ef01:2345:6789 huhuhu",
                      "ipv6", "abcd:ef01:2345:6789:abcd:ef01:2345:6789", NULL);

  test_search_matches(root, "bbb6 0:0:0:0:0:0:0:0 huhuhu",
                      "ipv6", "0:0:0:0:0:0:0:0", NULL);

  test_search_matches(root, "bbb6 2001:DB8::8:800:200C:417A huhuhu",
                      "ipv6", "2001:DB8::8:800:200C:417A", NULL);

  test_search_matches(root, "bbb6 FF01::101 huhuhu",
                      "ipv6", "FF01::101", NULL);

  test_search_matches(root, "bbb6 ::1 huhuhu",
                      "ipv6", "::1", NULL);

  test_search_matches(root, "bbb6 :: huhuhu",
                      "ipv6", "::", NULL);

  test_search_matches(root, "bbb6 0:0:0:0:0:0:13.1.68.3 huhuhu",
                      "ipv6", "0:0:0:0:0:0:13.1.68.3", NULL);

  test_search_matches(root, "bbb6 ::202.1.68.3 huhuhu",
                      "ipv6", "::202.1.68.3", NULL);

  test_search_matches(root, "bbb6 2001:0DB8:0:CD30:: huhuhu",
                      "ipv6", "2001:0DB8:0:CD30::", NULL);

  test_search_matches(root, "bbb ABCD:EF01:2345:6789:ABCD:EF01:2345:6789 huhuhu",
                      "ip", "ABCD:EF01:2345:6789:ABCD:EF01:2345:6789", NULL);

  test_search_matches(root, "bbb abcd:ef01:2345:6789:abcd:ef01:2345:6789 huhuhu",
                      "ip", "abcd:ef01:2345:6789:abcd:ef01:2345:6789", NULL);

  test_search_matches(root, "bbb :: huhuhu",
                      "ip", "::", NULL);

  test_search_matches(root, "bbb 0:0:0:0:0:0:13.1.68.3 huhuhu",
                      "ip", "0:0:0:0:0:0:13.1.68.3", NULL);

  test_search_matches(root, "bbb ::202.1.68.3 huhuhu",
                      "ip", "::202.1.68.3", NULL);

  test_search_matches(root, "bbb 2001:0DB8:0:CD30:: huhuhu",
                      "ip", "2001:0DB8:0:CD30::", NULL);

  test_search_matches(root, "ccc 'quoted string' hehehe",
                      "qstring", "quoted string",
                      NULL);

  test_search_matches(root, "ddd estring: hehehe",
                      "estring", "estring",
                      NULL);

  test_search_matches(root, "eee string hehehe",
                      "string", "string",
                      NULL);

  test_search_matches(root, "fff 12345 hihihi",
                      "float", "12345", NULL);

  test_search_matches(root, "fff 12345hihihi",
                      "float", "12345", NULL);

  test_search_matches(root, "fff 12.345hihihi",
                      "float", "12.345", NULL);

  test_search_matches(root, "fff 12.345.hihihi",
                      "float", "12.345", NULL);

  test_search_matches(root, "fff 12.345.6hihihi",
                      "float", "12.345", NULL);

  test_search_matches(root, "fff 12345.hihihi",
                      "float", "12345.", NULL);

  r_free_node(root, NULL);
}

void
test_zorp_logs(void)
{
  RNode *root = r_new_node("", NULL);

  /* these are conflicting logs */
  r_insert_node(root, strdup("core.error(2): (svc/@STRING:service:._@:@NUMBER:instance_id@/plug): Connection to remote end failed; local='AF_INET(@IPv4:local_ip@:@NUMBER:local_port@)', remote='AF_INET(@IPv4:remote_ip@:@NUMBER:remote_port@)', error=@QSTRING:errormsg:'@"), "ZORP", TRUE, NULL);
  r_insert_node(root, strdup("core.error(2): (svc/@STRING:service:._@:@NUMBER:instance_id@/plug): Connection to remote end failed; local=@QSTRING:p:'@, remote=@QSTRING:p:'@, error=@QSTRING:p:'@"), "ZORP1", TRUE, NULL);
  r_insert_node(root, strdup("Deny@QSTRING:FIREWALL.DENY_PROTO: @src@QSTRING:FIREWALL.DENY_O_INT: :@@IPv4$:FIREWALL.DENY_SRCIP@/@NUMBER:FIREWALL.DENY_SRCPORT@ dst"), "CISCO", TRUE, NULL);

  test_search_value(root, "core.error(2): (svc/intra.servers.alef_SSH_dmz.zajin:111/plug): Connection to remote end failed; local='AF_INET(172.16.0.1:56867)', remote='AF_INET(172.18.0.1:22)', error='No route to host'PAS", "ZORP");
  test_search_value(root, "Deny udp src OUTSIDE:10.0.0.0/1234 dst INSIDE:192.168.0.0/5678 by access-group \"OUTSIDE\" [0xb74026ad, 0x0]", "CISCO");

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
  test_matches();
  test_zorp_logs();

  app_shutdown();
  return  (fail ? 1 : 0);
}

