#include "persist-state.h"
#include "state.h"
#include "apphook.h"
#include "testutils.h"

#include <stdio.h>
#include <stdlib.h>

#define TEST_PERSIST_FILE "test_state.persist"
#define TEST_PREFIX "test_state"
#define TEST_VALUE "test_value"

typedef struct _TestState
{
  BaseState super;
  gchar value[15];
} TestState;

NameValueContainer *
test_state_dump_state(StateHandler *self)
{
  NameValueContainer *result = name_value_container_new();
  TestState *ts;
  ts = (TestState *) state_handler_get_state(self);
  name_value_container_add_int(result, "version", ts->super.version);
  name_value_container_add_boolean(result, "big_endian",
      ts->super.big_endian);
  name_value_container_add_string(result, "value", ts->value);
  state_handler_put_state(self);
  return result;
}

PersistEntryHandle
test_state_alloc_state(StateHandler *self)
{
  self->size = sizeof(TestState);
  return persist_state_alloc_entry(self->persist_state, self->name, self->size);
}

StateHandler *
create_test_state_handler(PersistState * state, const gchar *value)
{
  StateHandler *self = g_new0(StateHandler, 1);
  state_handler_init(self, state, value);
  self->dump_state = test_state_dump_state;
  self->alloc_state = test_state_alloc_state;
  return self;
}

void
test_state_dump()
{
  PersistState *state;
  TestState *test_state;
  PersistEntryHandle persist_handle;
  StateHandler *state_handler;
  NameValueContainer *dumped_value;
  gchar *value;
  STATE_HANDLER_CONSTRUCTOR constructor;

  unlink(TEST_PERSIST_FILE);
  state = persist_state_new(TEST_PERSIST_FILE);
  assert_true(persist_state_start(state),
      "Error starting persist_state object");

  persist_handle = persist_state_alloc_entry(state, TEST_PREFIX,
      sizeof(TestState));
  assert_true(persist_handle != 0, "Can't allocate persist_entry");

  constructor = state_handler_get_constructor_by_prefix(TEST_PREFIX);
  assert_true(constructor != NULL, "Can't get constructor");

  state_handler = constructor(state, TEST_PREFIX);
  assert_true(state_handler != NULL, "Can't create default PersistHandler");

  test_state = (TestState *) state_handler_get_state(state_handler);
  assert_true(test_state != NULL, "Can't map test state");

  memset(test_state, 0, sizeof(TestState));
  test_state->super.big_endian = FALSE;
  test_state->super.version = 1;
  memcpy(test_state->value, TEST_VALUE, strlen(TEST_VALUE));

  dumped_value = state_handler_dump_state(state_handler);
  value = name_value_container_get_value(dumped_value, "value");
  assert_true(value != NULL, "Invalid dumped value");
  assert_string(value, "01 00 74 65 73 74 5F 76 61 6C 75 65 00 00 00 00 00",
      "Invalid dumped value");
  g_free(value);
  name_value_container_free(dumped_value);

  state_handler_put_state(state_handler);
  state_handler_free(state_handler);

  state_handler = NULL;

  state_handler_register_constructor(TEST_PREFIX, create_test_state_handler);
  constructor = state_handler_get_constructor_by_prefix(TEST_PREFIX);
  assert_gpointer(constructor, create_test_state_handler, "Bad constructor");

  state_handler = constructor(state, TEST_PREFIX);
  assert_true(state_handler != NULL, "Can't create test state handler");
  dumped_value = state_handler_dump_state(state_handler);

  value = name_value_container_get_value(dumped_value, "version");
  assert_string(value, "1", "Bad value");
  g_free(value);

  value = name_value_container_get_value(dumped_value, "big_endian");
  assert_string(value, "false", "Bad value");
  g_free(value);

  value = name_value_container_get_value(dumped_value, "value");
  assert_string(value, TEST_VALUE, "Bad value");
  g_free(value);

  name_value_container_free(dumped_value);

  state_handler_put_state(state_handler);
  state_handler_free(state_handler);

  persist_state_commit(state);
  persist_state_free(state);
}

void
test_state_alloc()
{
  PersistState *state;
  TestState *test_state;
  PersistEntryHandle persist_handle;
  StateHandler *state_handler;
  NameValueContainer *dumped_value;
  gchar *value;
  STATE_HANDLER_CONSTRUCTOR constructor;

  unlink(TEST_PERSIST_FILE);
  state = persist_state_new(TEST_PERSIST_FILE);
  assert_true(persist_state_start(state),
      "Error starting persist_state object");
  state_handler_register_constructor(TEST_PREFIX, create_test_state_handler);

  constructor = state_handler_get_constructor_by_prefix(TEST_PREFIX);
  state_handler = constructor(state, TEST_PREFIX);

  persist_handle = state_handler_alloc_state(state_handler);
  assert_true(persist_handle != 0, "Can't allocate state");

  test_state = (TestState *) state_handler_get_state(state_handler);
  assert_true(test_state != NULL, "Can't get state after alloc");

  memset(test_state, 0, sizeof(TestState));
  test_state->super.big_endian = FALSE;
  test_state->super.version = 1;
  memcpy(test_state->value, TEST_VALUE, strlen(TEST_VALUE));

  state_handler_put_state(state_handler);

  test_state = (TestState *) state_handler_get_state(state_handler);

  dumped_value = state_handler_dump_state(state_handler);

  value = name_value_container_get_value(dumped_value, "version");
  assert_string(value, "1", "Bad value");
  g_free(value);

  value = name_value_container_get_value(dumped_value, "big_endian");
  assert_string(value, "false", "Bad value");
  g_free(value);

  value = name_value_container_get_value(dumped_value, "value");
  assert_string(value, TEST_VALUE, "Bad value");
  g_free(value);

  name_value_container_free(dumped_value);

  state_handler_put_state(state_handler);
  state_handler_free(state_handler);

  persist_state_commit(state);
  persist_state_free(state);
}

#define JSON_STRING "{ \"test_int64\": -4822678189205112, \"test_int\": 2003195204, \"test_boolean\": true, \"test_string\": \"test_value\"}"


void
test_name_value_container()
{
  NameValueContainer *container = name_value_container_new();
  gint64 value_int64 = 0xFFEEDDCCBBAA9988;
  gint value_int = 0x77665544;
  gchar *value_string = "test_value";
  gboolean value_boolean = TRUE;


  name_value_container_add_string(container, "test_string", value_string);
  name_value_container_add_int64(container, "test_int64", value_int64);
  name_value_container_add_int(container, "test_int", value_int);
  name_value_container_add_boolean(container, "test_boolean", value_boolean);

  assert_gint64(name_value_container_get_int64(container, "test_int64"), value_int64, "Bad int64 behavior");
  assert_gint(name_value_container_get_int(container, "test_int"), value_int, "Bad int behavior");
  assert_gboolean(name_value_container_get_boolean(container, "test_boolean"), value_boolean, "Bad boolean behavior");
  assert_string(name_value_container_get_string(container, "test_string"), value_string, "Bad string behavior");

  name_value_container_free(container);

  container = name_value_container_new();

  assert_true(name_value_container_parse_json_string(container, JSON_STRING),"Can't parse valid json string");
  assert_gint64(name_value_container_get_int64(container, "test_int64"), value_int64, "Bad int64 behavior");
  assert_gint(name_value_container_get_int(container, "test_int"), value_int, "Bad int behavior");
  assert_gboolean(name_value_container_get_boolean(container, "test_boolean"), value_boolean, "Bad boolean behavior");
  assert_string(name_value_container_get_string(container, "test_string"), value_string, "Bad string behavior");

  name_value_container_free(container);
}

int
main(int argc, char *argv[])
{
#if __hpux__
  return 0;
#endif
  app_startup();
  test_name_value_container();
  test_state_dump();
  test_state_alloc();

  return 0;
}
