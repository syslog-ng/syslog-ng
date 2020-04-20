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

#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <criterion/parameterized.h>

#include "apphook.h"
#include "logmsg/logmsg.h"
#include "messages.h"
#include "filter/filter-tags.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#define NUM_TAGS 8159
#define FILTER_TAGS 100

static gchar *
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

        cr_assert_eq(id, i, "Invalid tag id %d %s", id, name);

        g_free(name);
      }

  for (i = 0; i < NUM_TAGS; i++)
    {
      name = get_tag_by_id(i);

      tag_name = log_tags_get_by_id(i);
      cr_log_info("Looking up tag by id %d %s(%s)\n", i, tag_name, name);

      cr_assert_not_null(tag_name, "Error looking up tag by id %d %s\n", i, name);
      cr_assert_str_eq(name, tag_name, "Bad tag name for id %d %s (%s)\n", i, tag_name, name);

      g_free(name);
    }

  for (i = NUM_TAGS; i < (NUM_TAGS + 3); i++)
    {
      cr_log_info("Looking up tag by invalid id %d\n", i);
      tag_name = log_tags_get_by_id(i);
      cr_assert_not(tag_name, "Found tag name for invalid id %d %s\n", i, tag_name);
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
      for (i = 0; i < NUM_TAGS; i++)
        {
          name = get_tag_by_id(i);
          LogTagId id = log_tags_get_by_name(name);

          if (set)
            log_msg_set_tag_by_name(msg, name);
          else
            log_msg_clear_tag_by_name(msg, name);
          cr_log_info("%s tag %d %s\n", set ? "Setting" : "Clearing", id, name);

          cr_assert_not(set ^ log_msg_is_tag_by_id(msg, id), "Tag is %sset now (by id) %d", set ? "not " : "", id);

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

          cr_assert_not(set ^ log_msg_is_tag_by_id(msg, i), "Tag is %sset now (by id) %d\n", set ? "not " : "", i);

          cr_assert_not(set && i < sizeof(gulong) * 8
                        && msg->num_tags != 0, "Small IDs are set which should be stored in-line but num_tags is non-zero");

          g_free(name);
        }
    }
  log_msg_unref(msg);

}

Test(tags, test_filters_true)
{
  LogMessage *msg = log_msg_new_empty();
  FilterExprNode *f = filter_tags_new(NULL);
  guint i;
  GList *l = NULL;
  gchar *name;

  cr_log_info("=== filter tests not===\n");

  for (i = 1; i < FILTER_TAGS; i += 3)
    l = g_list_prepend(l, get_tag_by_id(i));

  filter_tags_add(f, l);
  f->comp = TRUE;

  for (i = 0; i < FILTER_TAGS; i++)
    {
      cr_log_info("Testing filter, message has tag %d\n", i);

      name = get_tag_by_id(i);
      LogTagId id = log_tags_get_by_name(name);
      g_free(name);

      log_msg_set_tag_by_id(msg, id);

      cr_assert(((i % 3 == 1) ^ filter_expr_eval(f, msg)), "Failed to match message by tag %d\n", id);

      cr_log_info("Testing filter, message no tag\n");

      log_msg_clear_tag_by_id(msg, id);

      cr_assert(filter_expr_eval(f, msg), "Failed to match message with no tags\n");
    }

  filter_expr_unref(f);
  log_msg_unref(msg);
}

Test(tags, test_filters_false)
{
  LogMessage *msg = log_msg_new_empty();
  FilterExprNode *f = filter_tags_new(NULL);
  guint i;
  GList *l = NULL;
  gchar *name;

  cr_log_info("=== filter tests ===\n");

  for (i = 1; i < FILTER_TAGS; i += 3)
    l = g_list_prepend(l, get_tag_by_id(i));

  filter_tags_add(f, l);
  f->comp = FALSE;

  for (i = 0; i < FILTER_TAGS; i++)
    {
      cr_log_info("Testing filter, message has tag %d\n", i);

      name = get_tag_by_id(i);
      LogTagId id = log_tags_get_by_name(name);
      g_free(name);

      log_msg_set_tag_by_id(msg, id);

      cr_assert_not(((i % 3 == 1) ^ filter_expr_eval(f, msg)), "Failed to match message by tag %d\n", id);

      cr_log_info("Testing filter, message no tag\n");

      log_msg_clear_tag_by_id(msg, id);

      cr_assert_not(filter_expr_eval(f, msg), "Failed to match message with no tags\n");
    }

  filter_expr_unref(f);
  log_msg_unref(msg);
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
