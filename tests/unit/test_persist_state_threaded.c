#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include "persist-state.h"
#include "apphook.h"
#include "atomic.h"

#ifdef __GNUC__
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#else
#define PRETTY_FUNCTION "[]"
#endif

#define TEST_ASSERT(assertion, msg) if (!(assertion)) \
                                      { \
                                          fprintf(stderr, "\n[TID:%x]%s :: %s\n\n", pthread_self(), PRETTY_FUNCTION, msg); \
                                          exit(1); \
                                      }

#define THREAD_NUM 6
#define DATA_SIZE 64
#define INITIAL_ENTRY_NUM 1000

gint NUMBER_OF_ENTRIES = INITIAL_ENTRY_NUM;
gint NUMBER_OF_ENTRIES_TO_BE_ADDED = 1e04;
gint RUN = 1;

PersistState *state;

static inline void
stop_threads()
{
  RUN = 0;
}

static inline int
microsleep(guint usec)
{
  struct timespec t_req, t_rem;

  t_req.tv_sec = usec / 1000000;
  t_req.tv_nsec = (usec % 1000000) * 1000;

  /* called only by main thread, thus errno is valid */
  while ((nanosleep(&t_req, &t_rem) < 0) && (errno == EINTR))
    t_req = t_rem;
}

void
add_entry_with_key_idx(int i)
{
  int j;
  gchar *data = NULL;
  gchar buf[32] = {0};
  PersistEntryHandle handle;

  g_snprintf(buf, sizeof(buf), "testkey%d", i);

  if (!(handle = persist_state_alloc_entry(state, buf, DATA_SIZE)))
    {
      fprintf(stderr, "Error allocating value in the persist file: %s\n", buf);
      exit(1);
    }

  data = persist_state_map_entry(state, handle);
  for (j = 0; j < DATA_SIZE; j++)
    {
      data[j] = (i % 26) + 'A';
    }
  persist_state_unmap_entry(state, handle);
}

void
assert_fail_if_entry_by_key_idx_not_found_or_containing_invalid_data(int i)
{
  int j;
  gchar *data = NULL;
  gchar buf[32] = {0};
  PersistEntryHandle handle;
  gsize size;
  guint8 version;

  g_snprintf(buf, sizeof(buf), "testkey%d", i);

  TEST_ASSERT((handle = persist_state_lookup_entry(state, buf, &size, &version)) != 0,
              "Error retrieving value from the persist file");

  data = persist_state_map_entry(state, handle);
  for (j = 0; j < DATA_SIZE; j++)
    {
      TEST_ASSERT(data[j] == (i % 26) + 'A', "Invalid data in persistent entry");
    }
  persist_state_unmap_entry(state, handle);
}

void
setup()
{
  int i;
  unlink("test_values.persist");
  state = persist_state_new("test_values.persist");
  if (!persist_state_start(state))
    {
      fprintf(stderr, "Error starting persist_state object\n");
      exit(1);
    }
  for (i = 0; i <NUMBER_OF_ENTRIES; i++)
    {
      add_entry_with_key_idx(i);
    }
}

void
teardown()
{
  persist_state_free(state);
}

void*
persist_state_map_unmap_threaded(void *arg)
{
/*  guint64 cycle_ctr = 0; */
  guint64 i, n;

  while (RUN)
  {
    n = g_atomic_int_get(&NUMBER_OF_ENTRIES);
/*    fprintf(stderr, "TID:[%x]N:%d; cycle_ctr:%d\n", pthread_self(), n, cycle_ctr); */
    for (i = 0; i < n; i++)
      {
        assert_fail_if_entry_by_key_idx_not_found_or_containing_invalid_data(i);
      }
/*    ++cycle_ctr; */
  }

  return NULL;
}

#define should_update_screen(x) (((x) & 0x7ff) == 0)

void
test_values()
{
  int i;
  guint64 n;
  pthread_t threads[THREAD_NUM] = {0};

  for (i = 0; i < THREAD_NUM; i++)
    pthread_create(&threads[i], NULL, persist_state_map_unmap_threaded, NULL);

  for (n = 0; n < NUMBER_OF_ENTRIES_TO_BE_ADDED; n++)
    {
      add_entry_with_key_idx(g_atomic_int_get(&NUMBER_OF_ENTRIES));
      g_atomic_int_inc(&NUMBER_OF_ENTRIES);
      if (should_update_screen(n))
        {
          fprintf(stderr, "[MAIN]N:%d\r", g_atomic_int_get(&NUMBER_OF_ENTRIES));
        }
      microsleep(10);
    }
  fprintf(stderr, "\n");

  stop_threads();

  for (i = 0; i < THREAD_NUM; i++)
    {
      fprintf(stderr, "waiting for TID[%x]...", threads[i]);
      pthread_join(threads[i], NULL);
      fprintf(stderr, "OK\n");
    }
}

int
main(int argc, char *argv[])
{
#if __hpux__
  return 0;
#endif

  app_startup();
  setup();
  test_values();
  teardown();

  return 0;
}
