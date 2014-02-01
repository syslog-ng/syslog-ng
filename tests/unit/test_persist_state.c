#include "persist-state.h"
#include "apphook.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include "libtest/testutils.h"
#include "libtest/persist_lib.h"

typedef struct _TestState
{
  guint32 value;
} TestState;

void
test_persist_state_open_success_on_invalid_file(void)
{
  PersistState *state;
  int fd;
  unlink("test_invalid_magic.persist");

  fd = open("test_invalid_magic.persist", O_CREAT | O_RDWR, 0777);
  write(fd, "aaa", 3);
  close(fd);

  state = persist_state_new("test_invalid_magic.persist");
  assert_true(persist_state_start(state), "persist_state_start returned with false!");

  cancel_and_destroy_persist_state(state);
}

void
test_persist_state_open_fails_on_invalid_file_with_dump(void)
{
  PersistState *state;
  int fd;
  unlink("test_invalid_magic.persist");

  fd = open("test_invalid_magic.persist", O_CREAT | O_RDWR, 0777);
  write(fd, "aaa", 3);
  close(fd);

  state = persist_state_new("test_invalid_magic.persist");
  assert_false(persist_state_start_dump(state), "persist_state_start_dump returned with success when persist file was invalid!");

  cancel_and_destroy_persist_state(state);
}

void
test_persist_state_open_failes_when_file_open_fails(void)
{
  PersistState *state;
  unlink("test_invalid_magic.persist");

  state = persist_state_new("test_invalid_magic.persist");

  assert_false(persist_state_start_dump(state), "persist_state_start_dump returned with success when persist file open failed!");

  cancel_and_destroy_persist_state(state);
}

void
write_test_file_for_test_in_use_handle(gboolean in_use_handle)
{
  PersistState *state = clean_and_create_persist_state_for_test("test_in_use.persist");
  PersistEntryHandle handle = persist_state_alloc_entry(state, "alma", sizeof(TestState));
  TestState *test_state = (TestState*) persist_state_map_entry(state, handle);
  test_state->value = 0xDEADBEEF;
  persist_state_unmap_entry(state, handle);

  if (!in_use_handle)
    persist_state_remove_entry(state, "alma");

  commit_and_free_persist_state(state);
}

void
test_persist_state_in_use_handle_is_loaded(void)
{
  PersistState *state;
  guint8 version;
  gsize size;
  PersistEntryHandle handle;

  unlink("test_in_use.persist");
  write_test_file_for_test_in_use_handle(TRUE);

  state = create_persist_state_for_test("test_in_use.persist");

  handle = persist_state_lookup_entry(state, "alma", &size, &version);

  assert_false(handle == 0, "lookup failed when looking for simple entry with in_use = TRUE!");

  cancel_and_destroy_persist_state(state);
}

void
test_persist_state_not_in_use_handle_is_not_loaded(void)
{
  PersistState *state;
  guint8 version;
  gsize size;
  PersistEntryHandle handle;

  unlink("test_in_use.persist");
  write_test_file_for_test_in_use_handle(FALSE);

  state = create_persist_state_for_test("test_in_use.persist");

  handle = persist_state_lookup_entry(state, "alma", &size, &version);

  assert_true(handle == 0, "lookup succeeded when looking for simple entry with in_use = FALSE!");

  cancel_and_destroy_persist_state(state);
}

void
test_persist_state_not_in_use_handle_is_loaded_in_dump_mode(void)
{
  PersistState *state;
  guint8 version;
  gsize size;
  PersistEntryHandle handle;

  unlink("test_in_use.persist");
  write_test_file_for_test_in_use_handle(FALSE);

  state = persist_state_new("test_in_use.persist");
  persist_state_start_dump(state);

  handle = persist_state_lookup_entry(state, "alma", &size, &version);

  assert_false(handle == 0, "lookup failed in dump mode when looking for simple entry with in_use = FALSE!");

  cancel_and_destroy_persist_state(state);
}


void
test_persist_state_remove_entry(void)
{
  guint8 version;
  gsize size;

  PersistState *state = clean_and_create_persist_state_for_test("test_persist_state_remove_entry.persist");

  PersistEntryHandle handle = persist_state_alloc_entry(state, "test", sizeof(TestState));

  handle = persist_state_lookup_entry(state, "test", &size, &version);
  assert_true(handle != 0, "lookup failed before removing entry");

  persist_state_remove_entry(state, "test");

  state = restart_persist_state(state);

  handle = persist_state_lookup_entry(state, "test", &size, &version);
  assert_true(handle == 0, "lookup succeeded after removing entry");

  cancel_and_destroy_persist_state(state);
}

void 
_foreach_callback_assertions(gchar* name, gint size, gpointer entry, gpointer userdata)
{
  assert_string((gchar *) userdata, "test_userdata", "Userdata is not passed correctly to foreach func!");
  assert_string(name, "test", "Name of persist entry does not match!");
  TestState *state = (TestState *) entry;
  assert_gint(state->value, 3, "Content of state does not match!");
  assert_gint(size, sizeof(TestState), "Size of state does not match!");
}

void 
test_persist_state_foreach_entry(void)
{
  PersistState *state = clean_and_create_persist_state_for_test("test_persist_foreach.persist");

  PersistEntryHandle handle = persist_state_alloc_entry(state, "test", sizeof(TestState));
  TestState *test_state = persist_state_map_entry(state, handle);
  test_state->value = 3;
  persist_state_unmap_entry(state, handle);

  persist_state_foreach_entry(state, _foreach_callback_assertions, "test_userdata");

  cancel_and_destroy_persist_state(state);
}

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

void
test_persist_state_temp_file_cleanup_on_cancel()
{
  PersistState *state = clean_and_create_persist_state_for_test("test_persist_state_temp_file_cleanup_on_cancel.persist");

  cancel_and_destroy_persist_state(state);

  assert_true(access("test_persist_state_temp_file_cleanup_on_cancel.persist", F_OK) != 0,
              "persist file is removed on destroy()");
  assert_true(access("test_persist_state_temp_file_cleanup_on_cancel.persist-", F_OK) != 0,
              "backup persist file is removed on destroy()");
}

void
test_persist_state_temp_file_cleanup_on_commit_destroy()
{
  PersistState *state = clean_and_create_persist_state_for_test("test_persist_state_temp_file_cleanup_on_commit_destroy.persist");

  commit_and_destroy_persist_state(state);

  assert_true(access("test_persist_state_temp_file_cleanup_on_commit_destroy.persist", F_OK) != 0,
              "persist file is removed on destroy(), even after commit");
  assert_true(access("test_persist_state_temp_file_cleanup_on_commit_destroy.persist-", F_OK) != 0,
              "backup persist file is removed on destroy(), even after commit");
}


int
main(int argc, char *argv[])
{
#if __hpux__
  return 0;
#endif
  app_startup();
  test_values();
  test_persist_state_remove_entry();
  test_persist_state_temp_file_cleanup_on_cancel();
  test_persist_state_temp_file_cleanup_on_commit_destroy();
  test_persist_state_foreach_entry();
  test_persist_state_open_success_on_invalid_file();
  test_persist_state_open_fails_on_invalid_file_with_dump();
  test_persist_state_open_failes_when_file_open_fails();
  test_persist_state_in_use_handle_is_loaded();
  test_persist_state_not_in_use_handle_is_not_loaded();
  test_persist_state_not_in_use_handle_is_loaded_in_dump_mode();
  test_persist_state_remove_entry();

  return 0;
}
