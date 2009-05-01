
#include "apphook.h"
#include "tags.h"
#include "logmsg.h"
#include "messages.h"
#include "filter.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

gboolean fail = FALSE;
gboolean verbose = FALSE;

#define NUM_TAGS 8159
#define FILTER_TAGS 100

#define test_fail(fmt, args...) \
do {\
 printf(fmt, ##args); \
 fail = TRUE; \
} while (0);

#define test_msg(fmt, args...) \
do { \
  if (verbose) printf(fmt, ##args); \
} while (0);

gchar *
get_tag_by_id(guint id)
{
  return g_strdup_printf("tags%d", id);
}

void
test_tags(void)
{
  guint i, check;
  guint id;
  gchar *name, *tag_name;

  for (check = 0; check < 2; check++)
    for (i = 0; i < NUM_TAGS; i++)
      {
        name = get_tag_by_id(i);
        id = log_tags_get_by_name(name);
        test_msg("%s tag %s %d\n", check ? "Checking" : "Adding", name, id);
        if (id != i)
          test_fail("Invalid tag id %d %s\n", id, name);

        g_free(name);
      }

  for (i = 0; i < NUM_TAGS; i++)
    {
      name = get_tag_by_id(i);

      tag_name = log_tags_get_by_id(i);
      test_msg("Looking up tag by id %d %s(%s)\n", i, tag_name, name);
      if (tag_name)
        {
          if (!g_str_equal(tag_name, name))
            test_fail("Bad tag name for id %d %s (%s)\n", i, tag_name, name);
        }
      else
        test_fail("Error looking up tag by id %d %s\n", i, name);

      g_free(name);
    }

  for (i = NUM_TAGS; i < (NUM_TAGS + 3); i++)
    {
      tag_name = log_tags_get_by_id(i);
      test_msg("Looking up tag by invalid id %d\n", i);
      if (tag_name)
        test_fail("Found tag name for invalid id %d %s\n", i, tag_name);
    }
}

void
test_msg_tags()
{
  LogMessage *msg = log_msg_new_empty();
  gchar *name;
  gint i, set;

  test_msg("=== LogMessage tests ===\n");

  for (set = 1; set != -1; set--)
    {
      for (i = NUM_TAGS; i > -1; i--)
        {
          name = get_tag_by_id(i);
          if (set)
            log_msg_set_tag_by_name(msg, name);
          else
            log_msg_clear_tag_by_name(msg, name);
          test_msg("%s tag %d %s\n", set ? "Setting" : "Clearing", i, name);

          if (set ^ log_msg_is_tag_by_id(msg, i))
            test_fail("Tag is %sset now (by id) %d\n", set ? "not " : "", i);

          g_free(name);
        }
    
      for (i = 0; i < NUM_TAGS; i++)
        {
          name = get_tag_by_id(i);
          test_msg("Checking tag %d %s\n", i, name);
    
          if (set ^ log_msg_is_tag_by_id(msg, i))
            test_fail("Tag is %sset (by id) %d\n", set ? "not " : "", i);
          if (set ^ log_msg_is_tag_by_name(msg, name))
            test_fail("Tag is %sset (by name) %s\n", set ? "not " : "", name);
          
          g_free(name);
        }
    }

  log_msg_unref(msg);
}

void
test_filters(void)
{
  LogMessage *msg = log_msg_new_empty();
  FilterExprNode *f = filter_tags_new(NULL);
  guint i;
  GList *l = NULL;

  test_msg("=== filter tests ===\n");

  for (i = 1; i < FILTER_TAGS; i += 3)
    l = g_list_prepend(l, get_tag_by_id(i));

  filter_tags_add(f, l);

  for (i = 0; i < FILTER_TAGS; i++)
    {
      test_msg("Testing filter, message has tag %d\n", i);
      log_msg_set_tag_by_id(msg, i);

      if (((i % 3 == 1) ^ filter_expr_eval(f, msg)))
        test_fail("Failed to match message by tag %d\n", i);

      test_msg("Testing filter, message no tag\n");
      log_msg_clear_tag_by_id(msg, i);
      if (filter_expr_eval(f, msg))
        test_fail("Failed to match message with no tags\n");
    }

  filter_expr_free(f);
  log_msg_unref(msg);
}

int
main(int argc, char *argv[])
{
  app_startup();
  
  if (argc > 1)
    verbose = TRUE;

  msg_init(TRUE);
  
  test_tags();
  test_msg_tags();
  test_filters();

  app_shutdown();
  return  (fail ? 1 : 0);
}

