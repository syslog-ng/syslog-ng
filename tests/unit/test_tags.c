
#include "apphook.h"
#include "tags.h"
#include "messages.h"

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

int
main(int argc, char *argv[])
{
  app_startup();
  
  if (argc > 1)
    verbose = TRUE;

  msg_init(TRUE);
  
  test_tags();

  app_shutdown();
  return  (fail ? 1 : 0);
}

