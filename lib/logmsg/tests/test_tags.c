/*
 * Copyright (c) 2009-2016 Balabit
 * Copyright (c) 2009 Marton Illes
 * Copyright (c) 2009-2016 Balázs Scheidler
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

#include "apphook.h"
#include "logmsg/logmsg.h"
#include "messages.h"
#include "filter/filter-tags.h"
#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#define NUM_TAGS 8159
#define FILTER_TAGS 100

gchar *
get_tag_by_id(LogTagId id)
{
  return g_strdup_printf("tags%d", id);
}

Test(tags, test_tags)
{
  guint i, check;
  guint id;
  gchar *name;
  const gchar *tag_name;

  for (check = 0; check < 2; check++)
    for (i = 0; i < NUM_TAGS; i++)
      {
        name = get_tag_by_id(i);
        id = log_tags_get_by_name(name);
        cr_log_info("%s tag %s %d\n", check ? "Checking" : "Adding", name, id);
        if (id != i)
          cr_assert_eq(id, i, "Invalid tag id %d %s\n", id, name);

        g_free(name);
      }

  for (i = 0; i < NUM_TAGS; i++)
    {
      name = get_tag_by_id(i);

      tag_name = log_tags_get_by_id(i);
      cr_log_info("Looking up tag by id %d %s(%s)\n", i, tag_name, name);
      if (tag_name)
        {
          cr_assert_str_eq(tag_name, name, "Bad tag name for id %d %s (%s)\n", i, tag_name, name);
        }
      else
        cr_log_info("Error looking up tag by id %d %s\n", i, name);

      g_free(name);
    }

  for (i = NUM_TAGS; i < (NUM_TAGS + 3); i++)
    {
      tag_name = log_tags_get_by_id(i);
      cr_log_info("Looking up tag by invalid id %d\n", i);
      if (tag_name)
        cr_log_info("Found tag name for invalid id %d %s\n", i, tag_name);
    }
}

Test(tags, test_msg_tags)
{
  gchar *name;
  gint i, set;

  cr_log_info("=== LogMessage tests ===\n");

  LogMessage *msg = log_msg_new_empty();
  for (set = 1; set != -1; set--)
    {
      for (i = NUM_TAGS; i > -1; i--)
        {
          name = get_tag_by_id(i);

          if (set)
            log_msg_set_tag_by_name(msg, name);
          else
            log_msg_clear_tag_by_name(msg, name);
          cr_log_info("%s tag %d %s\n", set ? "Setting" : "Clearing", i, name);

          if (set ^ log_msg_is_tag_by_id(msg, i))
            cr_log_info("Tag is %sset now (by id) %d\n", set ? "not " : "", i);

          g_free(name);
        }
    }
  log_msg_unref(msg);

  msg = log_msg_new_empty();
  for (set = 1; set != -1; set--)
    {
      for (i = 0; i < NUM_TAGS; i++)
        {
          name = get_tag_by_id(i);

          if (set)
            log_msg_set_tag_by_name(msg, name);
          else
            log_msg_clear_tag_by_name(msg, name);

          cr_log_info("%s tag %d %s\n", set ? "Setting" : "Clearing", i, name);

          if (set ^ log_msg_is_tag_by_id(msg, i))
            cr_log_info("Tag is %sset now (by id) %d\n", set ? "not " : "", i);

          if (set && i < sizeof(gulong) * 8 && msg->num_tags != 0)
            cr_log_info("Small IDs are set which should be stored in-line but num_tags is non-zero");

          g_free(name);
        }
    }
  log_msg_unref(msg);

}

void
test_filters(gboolean not)
{
  LogMessage *msg = log_msg_new_empty();
  FilterExprNode *f = filter_tags_new(NULL);
  guint i;
  GList *l = NULL;

  cr_log_info("=== filter tests %s===\n", not ? "not " : "");

  for (i = 1; i < FILTER_TAGS; i += 3)
    l = g_list_prepend(l, get_tag_by_id(i));

  filter_tags_add(f, l);
  f->comp = not;

  for (i = 0; i < FILTER_TAGS; i++)
    {
      cr_log_info("Testing filter, message has tag %d\n", i);
      log_msg_set_tag_by_id(msg, i);

      if (((i % 3 == 1) ^ filter_expr_eval(f, msg)) ^ not)
        cr_log_info("Failed to match message by tag %d\n", i);

      cr_log_info("Testing filter, message no tag\n");
      log_msg_clear_tag_by_id(msg, i);
      if (filter_expr_eval(f, msg) ^ not)
        cr_log_info("Failed to match message with no tags\n");
    }

  filter_expr_unref(f);
  log_msg_unref(msg);
}


typedef struct
{
  gboolean not;
} TestFilters;

ParameterizedTestParameters(tags, test_filters)
{
  static TestFilters test_cases[] =
  {
    {TRUE},
    {FALSE}
  };

  return cr_make_param_array(TestFilters, test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
}

ParameterizedTest(TestFilters *test_cases, tags, test_filters)
{
  test_filters(test_cases->not);
}

static void
setup(void)
{
  app_startup();
  msg_init(TRUE);
}

static void
teardown(void)
{
  app_shutdown();
}

TestSuite(tags, .init = setup, .fini = teardown);
