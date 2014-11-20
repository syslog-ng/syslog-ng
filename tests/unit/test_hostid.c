#include "testutils.h"
#include "syslog-ng.h"
#include "host-id.h"
#include "logmsg.h"
#include "misc.h"
#include "apphook.h"
#include "cfg.h"
#include "mainloop.h"
#include <stdio.h>
#include <string.h>

#define HOSTID_TESTCASE(testfunc, ...)  { testcase_begin("%s(%s)", #testfunc, #__VA_ARGS__); testfunc(__VA_ARGS__); testcase_end(); }

#define PERSIST_FILE "/tmp/test_hostid.persist"

static GlobalConfig *
create_cfg()
{
  GlobalConfig *cfg;

  cfg = cfg_new(0x0302);

  return cfg;
}

static PersistState *
create_persist_state(const gchar *persist_file)
{
  PersistState *state;

  state = persist_state_new(persist_file);

  assert_true(persist_state_start(state) == TRUE,
              "Error starting persist state object [%s]\n",
              persist_file);
  return state;
}

static guint32
load_hostid_from_persist(const gchar *persist_file)
{
  PersistState *state;
  PersistEntryHandle handle;
  HostIdState *host_id_state;
  gsize size;
  guint8 version;

  state = create_persist_state(persist_file);
  handle = persist_state_lookup_entry(state, HOST_ID_PERSIST_KEY, &size, &version);
  assert_true(handle != 0, "cannot find hostid in persist file");
  host_id_state = persist_state_map_entry(state, handle);

  return host_id_state->host_id;
}

static void
init_mainloop_with_persist_file(const gchar *persist_file)
{
  GlobalConfig *cfg;

  cfg = create_cfg();

  assert_true(main_loop_initialize_state(cfg, persist_file) == TRUE,
             "main_loop_initialize_state failed");

  cfg_free(cfg);
}

static void
init_mainloop_with_newly_created_persist_file(const gchar *persist_file)
{
  unlink(persist_file);

  init_mainloop_with_persist_file(persist_file);
}

static void
create_persist_file_with_hostid(const gchar *persist_file, guint32 hostid)
{
  PersistState *state;
  PersistEntryHandle handle;
  HostIdState *host_id_state;

  unlink(persist_file);
  state = create_persist_state(persist_file);
  handle = persist_state_alloc_entry(state, HOST_ID_PERSIST_KEY, sizeof(HostIdState));
  host_id_state = persist_state_map_entry(state, handle);
  host_id_state->host_id = hostid;
  persist_state_commit(state);
}

static void
test_if_hostid_generated_when_persist_file_not_exists()
{
  guint32 hostid;
  init_mainloop_with_newly_created_persist_file(PERSIST_FILE);

  hostid = load_hostid_from_persist(PERSIST_FILE);

  assert_true(hostid == host_id_get(),
              "read hostid(%u) differs from the newly generated hostid(%u)",
              hostid,
              host_id_get());
}

static void
test_if_hostid_remain_unchanged_when_persist_file_exists()
{
  static const int hostid = 323;
  create_persist_file_with_hostid(PERSIST_FILE, hostid);

  init_mainloop_with_persist_file(PERSIST_FILE);

  assert_true(host_id_get() == hostid,
              "loaded hostid(%d) differs from expected (%d)",
              host_id_get(),
              hostid);
}

int main(int argc, char **argv)
{
#if __hpux__
  return 0;
#endif
  app_startup();

  HOSTID_TESTCASE(test_if_hostid_generated_when_persist_file_not_exists);
  HOSTID_TESTCASE(test_if_hostid_remain_unchanged_when_persist_file_exists);

  app_shutdown();

  return 0;
}
