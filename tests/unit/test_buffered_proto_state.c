#include "persist-state.h"
#include "state.h"
#include "logproto.h"
#include "apphook.h"
#include "testutils.h"

#include <stdio.h>
#include <stdlib.h>

#define TEST_PERSIST_FILE "test_buffeted_proto_state.persist"
#define TEST_PREFIX "test_state"
#define TEST_VALUE_NAME "test_state(a.txt)"

#define TEST_G_RUN_ID 16
#define TEST_FILE_SIZE 150

#include "basic-proto.c"

typedef struct _TestProto
{
  LogProtoBufferedServer super;
} TestProto;

void
test_proto_apply_state(LogProto *s, StateHandler *state_handler)
{
  TestProto *self = (TestProto *) s;
  if (self->super.state_handler)
    {
      state_handler_free(self->super.state_handler);
    }
  self->super.state_handler = state_handler;
}

LogProto *
test_proto_new()
{
  TestProto *self = g_new0(TestProto, 1);

  self->super.super.convert = (GIConv) -1;

  self->super.super.apply_state = test_proto_apply_state;
  self->super.super.restart_with_state =
      log_proto_buffered_server_restart_with_state;
  self->super.super.free_fn = log_proto_buffered_server_free;
  return &self->super.super;
}

void
test_swap_bytes_of_state(LogProtoBufferedServerState *state)
{
  state->super.big_endian = !state->super.big_endian;
  state->buffer_pos = GUINT32_SWAP_LE_BE(state->buffer_pos);
  state->pending_buffer_pos = GUINT32_SWAP_LE_BE(state->pending_buffer_pos);
  state->pending_buffer_end = GUINT32_SWAP_LE_BE(state->pending_buffer_end);
  state->buffer_size = GUINT32_SWAP_LE_BE(state->buffer_size);
  state->buffer_cached_eol = GUINT32_SWAP_LE_BE(state->buffer_cached_eol);
  state->raw_stream_pos = GUINT64_SWAP_LE_BE(state->raw_stream_pos);
  state->raw_buffer_size = GUINT32_SWAP_LE_BE(state->raw_buffer_size);
  state->pending_raw_stream_pos =
      GUINT64_SWAP_LE_BE(state->pending_raw_stream_pos);
  state->pending_raw_buffer_size =
      GUINT32_SWAP_LE_BE(state->pending_raw_buffer_size);
  state->file_size = GUINT64_SWAP_LE_BE(state->file_size);
  state->file_inode = GUINT64_SWAP_LE_BE(state->file_inode);
  state->run_id = GUINT32_SWAP_LE_BE(state->run_id);
}

void
test_state_v4()
{
  PersistState *state;
  LogProtoBufferedServerState *proto_state;
  StateHandler *handler;

  TestProto *proto = (TestProto *) test_proto_new();

  unlink(TEST_PERSIST_FILE);
  state = persist_state_new(TEST_PERSIST_FILE);
  assert_true(persist_state_start(state),
      "Error starting persist_state object");

  /* Create new state */
  assert_true(
      log_proto_restart_with_state(&proto->super.super, state, TEST_VALUE_NAME),
      "Can't restart with empty state");

  handler = proto->super.state_handler;
  assert_true(handler != NULL, "Proto has no state handler");
  proto_state = (LogProtoBufferedServerState *) state_handler_get_state(handler);
  assert_true(proto_state != NULL, "Can't reach the state");
  assert_guint16((guint16)(proto_state->super.version), 1,
      "Bad version number");
  assert_true(proto_state->super.big_endian == (G_BYTE_ORDER == G_BIG_ENDIAN),
      "Bad byte order");

  /* Convert old state version from different endiannes */
  proto_state->file_size = TEST_FILE_SIZE;
  test_swap_bytes_of_state(proto_state);
  assert_guint64(proto_state->file_size, GUINT64_SWAP_LE_BE(TEST_FILE_SIZE),
      "LE=>BE swap failed");
  proto_state->super.version = 0;
  state_handler_put_state(handler);

  g_run_id = GUINT32_SWAP_LE_BE(TEST_G_RUN_ID) + 1;
  assert_true(
      log_proto_restart_with_state(&proto->super.super, state, TEST_VALUE_NAME),
      "Can't restart with filled state");

  handler = proto->super.state_handler;
  assert_true(handler != NULL, "Proto has no state handler");
  proto_state = (LogProtoBufferedServerState *) state_handler_get_state(
      handler);
  assert_true(proto_state->super.big_endian == (G_BYTE_ORDER == G_BIG_ENDIAN),
      "Bad byte order");
  assert_guint64(proto_state->file_size, TEST_FILE_SIZE, "Bad file size");
  assert_guint32(proto_state->run_id, TEST_G_RUN_ID,
      "Bad run id (Bad conversion)");
  state_handler_put_state(handler);

  log_proto_free(&proto->super.super);
  persist_state_commit(state);
  persist_state_free(state);
}

#define V3_STATE_FILE_POS 100
#define V3_STATE_FILE_INODE 10
#define V3_STATE_SIZE 1024

void
fill_v3_state(gpointer old_state, gsize state_size)
{
  GString *buffer = g_string_new("");
  SerializeArchive *archive = serialize_buffer_archive_new(old_state,
      state_size);
  serialize_write_uint32(archive, state_size - 4);
  serialize_write_uint16(archive, 0);
  serialize_write_uint64(archive, V3_STATE_FILE_POS);
  serialize_write_uint64(archive, V3_STATE_FILE_INODE);
  serialize_write_uint64(archive, TEST_FILE_SIZE);
  serialize_write_uint16(archive, 0);
  serialize_write_string(archive, buffer);
  serialize_archive_free(archive);
  g_string_free(buffer, TRUE );
}

void
test_state_v3()
{
  PersistState *state;
  PersistEntryHandle handle;
  LogProtoBufferedServerState *proto_state;
  StateHandler *handler;
  gpointer old_state;
  gsize old_state_size = V3_STATE_SIZE;

  TestProto *proto = (TestProto *) test_proto_new();

  unlink(TEST_PERSIST_FILE);
  state = persist_state_new(TEST_PERSIST_FILE);
  assert_true(persist_state_start(state),
      "Error starting persist_state object");

  handle = persist_state_alloc_entry(state, TEST_VALUE_NAME, old_state_size);
  handler = log_proto_buffered_server_state_handler_new(state, TEST_VALUE_NAME);
  old_state = state_handler_get_state(handler);

  assert_guint32(handle, handler->persist_handle, "Bad allocation");

  fill_v3_state(old_state, old_state_size);
  g_run_id = TEST_G_RUN_ID;
  assert_true(
      log_proto_buffered_server_update_server_state(&proto->super, handler, 3),
      "Can't convert the state");
  handler = proto->super.state_handler;
  proto_state = (LogProtoBufferedServerState *) state_handler_get_state(
      handler);

  assert_guint64(proto_state->file_inode, V3_STATE_FILE_INODE,
      "Bad conversion");
  assert_guint64(proto_state->file_size, TEST_FILE_SIZE, "Bad conversion");
  assert_guint64(proto_state->raw_stream_pos, V3_STATE_FILE_POS,
      "Bad conversion");
  assert_guint32(proto_state->run_id, TEST_G_RUN_ID - 1, "Bad conversion");
  assert_guint16((guint16)(proto_state->super.version), 1, "Bad conversion");
  assert_true(proto_state->super.big_endian == (G_BYTE_ORDER == G_BIG_ENDIAN),
      "Bad conversion");

  log_proto_free(&proto->super.super);
  persist_state_commit(state);
  persist_state_free(state);
}

#define STATE_NAME "affile_sd_curpos(/var/tmp/manylog.txt)"
#define JSON_STATE "{ \"version\": 1, \"big_endian\": false, \"raw_buffer_leftover_size\": 0, \"buffer_pos\": 8082, \"pending_buffer_end\": 0, \"buffer_size\": 8192, \"buffer_cached_eol\": 0, \"pending_buffer_pos\": 0, \"raw_stream_pos\": 122730, \"pending_raw_stream_pos\": 130812, \"raw_buffer_size\": 8082, \"pending_raw_buffer_size\": 0, \"file_size\": 130812, \"file_inode\": 11978166, \"run_id\": 2 }"
#define JSON_BAD_STATE "{ \"version\": 1, \"raw_buffer_leftover_size\": 0, \"buffer_pos\": 8082, \"pending_buffer_end\": 0, \"buffer_size\": 8192, \"buffer_cached_eol\": 0, \"pending_buffer_pos\": 0, \"raw_stream_pos\": 122730, \"pending_raw_stream_pos\": 130812, \"raw_buffer_size\": 8082, \"pending_raw_buffer_size\": 0, \"file_size\": 130812, \"file_inode\": 11978166, \"run_id\": 2 }"

void
test_load_state()
{
  PersistState *state;
  PersistEntryHandle handle;
  LogProtoBufferedServerState *proto_state;
  StateHandler *handler;
  NameValueContainer *new_state;

  TestProto *proto = (TestProto *) test_proto_new();

  unlink(TEST_PERSIST_FILE);
  state = persist_state_new(TEST_PERSIST_FILE);
  assert_true(persist_state_start(state),
      "Error starting persist_state object");

  /* Create new state */
  assert_true(log_proto_restart_with_state(&proto->super.super, state, TEST_VALUE_NAME),
      "Can't restart with empty state");

  handler = proto->super.state_handler;
  assert_true(handler != NULL, "Proto has no state handler");

  new_state = name_value_container_new();
  assert_true(name_value_container_parse_json_string(new_state, JSON_STATE), "Can't load valid json string");

  assert_true(state_handler_load_state(handler, new_state, NULL), "Can't load valid state");

  name_value_container_free(new_state);
  log_proto_free(proto);
  persist_state_commit(state);
  persist_state_free(state);

  proto = (TestProto *) test_proto_new();

  state = persist_state_new(TEST_PERSIST_FILE);
  assert_true(persist_state_start(state),
        "Error starting persist_state object");

  assert_true(log_proto_restart_with_state(&proto->super.super, state, TEST_VALUE_NAME),
        "Can't restart with empty state");

  handler = proto->super.state_handler;
  assert_true(handler != NULL, "Proto has no state handler");
  proto_state = state_handler_get_state(handler);

  assert_gint(proto_state->super.version, 1, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_gboolean(proto_state->super.big_endian, FALSE, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_gint(proto_state->raw_buffer_leftover_size, 0, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_guint32(proto_state->buffer_pos, 8082, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_guint32(proto_state->pending_buffer_end, 0, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_guint32(proto_state->buffer_size, 8192, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_guint32(proto_state->buffer_cached_eol, 0, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_guint32(proto_state->pending_buffer_pos, 0, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_guint64(proto_state->raw_stream_pos, 122730, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_guint64(proto_state->pending_raw_stream_pos, 130812, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_guint32(proto_state->raw_buffer_size, 8082, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_guint32(proto_state->pending_raw_buffer_size, 0, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_guint64(proto_state->file_size, 130812, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_guint64(proto_state->file_inode, 11978166, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);
  assert_guint32(proto_state->run_id, 2, "Bad saved data %s(%s:%d)", __FUNCTION__, __FILE__, __LINE__);

  log_proto_free(proto);
  persist_state_commit(state);
  persist_state_free(state);

  unlink(TEST_PERSIST_FILE);
  state = persist_state_new(TEST_PERSIST_FILE);
  assert_true(persist_state_start(state),
        "Error starting persist_state object");

  proto = (TestProto *) test_proto_new();
  /* Create new state */
  assert_true(log_proto_restart_with_state(&proto->super.super, state, TEST_VALUE_NAME),
        "Can't restart with empty state");

  handler = proto->super.state_handler;
  assert_true(handler != NULL, "Proto has no state handler");

  new_state = name_value_container_new();
  assert_true(name_value_container_parse_json_string(new_state, JSON_BAD_STATE), "Can't load valid json string");

  assert_false(state_handler_load_state(handler, new_state, NULL), "insufficient state loaded");

  name_value_container_free(new_state);
  log_proto_free(proto);
  persist_state_commit(state);
  persist_state_free(state);
}

int
main(int argc, char *argv[])
{
#if __hpux__
  return 0;
#endif
  app_startup();
  /*test_state_v4();
  test_state_v3();*/
  test_load_state();
  return 0;
}
