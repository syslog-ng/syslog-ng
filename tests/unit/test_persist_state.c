#include "persist-state.h"
#include "apphook.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void
test_values(void)
{
  PersistState *state;
  gint i, j;
  gchar *data;

  state = clean_and_create_persist_state_for_test("test_values.persist");

  for (i = 0; i < 1000; i++)
    {
      gchar buf[16];
      PersistEntryHandle handle;

      g_snprintf(buf, sizeof(buf), "testkey%d", i);
      if (!(handle = persist_state_alloc_entry(state, buf, 128)))
        {
          fprintf(stderr, "Error allocating value in the persist file: %s\n", buf);
          exit(1);
        }
      data = persist_state_map_entry(state, handle);
      for (j = 0; j < 128; j++)
        {
          data[j] = (i % 26) + 'A';
        }
      persist_state_unmap_entry(state, handle);
    }
  for (i = 0; i < 1000; i++)
    {
      gchar buf[16];
      PersistEntryHandle handle;
      gsize size;
      guint8 version;

      g_snprintf(buf, sizeof(buf), "testkey%d", i);
      if (!(handle = persist_state_lookup_entry(state, buf, &size, &version)))
        {
          fprintf(stderr, "Error retrieving value from the persist file: %s\n", buf);
          exit(1);
        }
      data = persist_state_map_entry(state, handle);
      for (j = 0; j < 128; j++)
        {
          if (data[j] != (i % 26) + 'A')
            {
              fprintf(stderr, "Invalid data in persistent entry\n");
              exit(1);
            }
        }
      persist_state_unmap_entry(state, handle);
    }

  state = restart_persist_state(state);

  for (i = 0; i < 1000; i++)
    {
      gchar buf[16];
      PersistEntryHandle handle;
      gsize size;
      guint8 version;

      g_snprintf(buf, sizeof(buf), "testkey%d", i);
      if (!(handle = persist_state_lookup_entry(state, buf, &size, &version)))
        {
          fprintf(stderr, "Error retrieving value from the persist file: %s\n", buf);
          exit(1);
        }
      if (size != 128 || version != 4)
        {
          fprintf(stderr, "Error retrieving value from the persist file: %s, invalid size (%d) or version (%d)\n", buf, (gint) size, version);
          exit(1);
        }
      data = persist_state_map_entry(state, handle);
      for (j = 0; j < 128; j++)
        {
          if (data[j] != (i % 26) + 'A')
            {
              fprintf(stderr, "Invalid data in persistent entry\n");
              exit(1);
            }
        }
      persist_state_unmap_entry(state, handle);
    }
  cancel_and_destroy_persist_state(state);
}

int
main(int argc, char *argv[])
{
#if __hpux__
  return 0;
#endif
  app_startup();
  test_values();
  return 0;
}
