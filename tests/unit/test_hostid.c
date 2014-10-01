#include "testutils.h"
#include "syslog-ng.h"
#include "logmsg.h"
#include "misc.h"
#include "apphook.h"
#include "cfg.h"
#include "center.h"
#include "mainloop.h"
#include "serialize.h"
#include <stdio.h>
#include <string.h>

#define HOSTID_TESTCASE(testfunc, ...)  { testcase_begin("%s(%s)", #testfunc, #__VA_ARGS__); testfunc(__VA_ARGS__); testcase_end(); }

#define PERSIST_FILE "test_hostid.persist"

static GlobalConfig *
create_cfg()
{
  GlobalConfig *cfg;

  cfg = cfg_new(VERSION_VALUE_3_2);
  cfg->center = log_center_new();

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
  guint32 *hostid_ptr;
  guint32 hostid;
  gsize size;
  guint8 version;

  state = create_persist_state(persist_file);
  handle = persist_state_lookup_entry(state, "hostid", &size, &version);
  assert_true(handle != 0, "cannot find hostid in persist file");
  hostid_ptr = persist_state_map_entry(state, handle);
  hostid = *hostid_ptr;
  return hostid;
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
  guint32 *hostid_ptr;

  unlink(persist_file);
  state = create_persist_state(persist_file);
  handle = persist_state_alloc_entry(state, "hostid", sizeof(guint32));
  hostid_ptr = persist_state_map_entry(state, handle);
  *hostid_ptr = hostid;
  persist_state_commit(state);
}

static void
test_if_hostid_generated_when_persist_file_not_exists()
{
  guint32 hostid;
  init_mainloop_with_newly_created_persist_file(PERSIST_FILE);

  hostid = load_hostid_from_persist(PERSIST_FILE);

  assert_true(hostid == g_hostid,
              "read hostid(%u) differs from the newly generated hostid(%u)",
              hostid,
              g_hostid);
}


static void
test_if_hostid_remain_unchanged_when_persist_file_exists()
{
  static const int hostid = 323;
  create_persist_file_with_hostid(PERSIST_FILE, hostid);

  init_mainloop_with_persist_file(PERSIST_FILE);

  assert_true(g_hostid == hostid,
              "loaded hostid(%d) differs from expected (%d)",
              g_hostid,
              hostid);
}

static void
test_if_hostid_is_serialized()
{
  static const guint32 G_HOSTID = 1234;
  static const guint32 TEST_HOSTID = 12345;
  LogMessage *msg, *read_msg;
  SerializeArchive *sa;
  GString *stream;

  create_persist_file_with_hostid(PERSIST_FILE, G_HOSTID);
  init_mainloop_with_persist_file(PERSIST_FILE);

  msg = log_msg_new_mark();

  assert_true(msg->hostid == G_HOSTID,
              "msg->hostid(%d) != g_hostid(%d), hostid is not set by log_msg_init()",
              msg->hostid,
              G_HOSTID);

  msg->hostid = TEST_HOSTID;
  stream = g_string_new("");
  sa = serialize_string_archive_new(stream);

  log_msg_write(msg, sa);
  log_msg_unref(msg);
  serialize_archive_free(sa);

  read_msg = log_msg_new_empty();
  sa  = serialize_string_archive_new(stream);
  log_msg_read(read_msg, sa);
  serialize_archive_free(sa);

  assert_true(read_msg->hostid == TEST_HOSTID,
              "read_msg->hostid(%d) != test_hostid(%d), wrong hostid is not read from serialized archived",
              read_msg->hostid,
              TEST_HOSTID);
  g_string_free(stream, TRUE);
  log_msg_unref(read_msg);
}

int main(int argc, char **argv)
{
#if __hpux__
  return 0;
#endif
  app_startup();

  HOSTID_TESTCASE(test_if_hostid_generated_when_persist_file_not_exists);
  HOSTID_TESTCASE(test_if_hostid_remain_unchanged_when_persist_file_exists);
  HOSTID_TESTCASE(test_if_hostid_is_serialized);

  app_shutdown();

  return 0;
}
