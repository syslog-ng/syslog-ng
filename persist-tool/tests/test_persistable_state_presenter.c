/*
 * Copyright (c) 2002-2014 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2014 Viktor Juhasz
 * Copyright (c) 2014 Viktor Tusa
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

#include "persist-state.h"
#include "persist-tool/json-presented-persistable-state.h"
#include "lib/persistable-state-presenter.h"
#include "apphook.h"
#include "testutils.h"
#include "libtest/persist_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PERSIST_FILE "test_state.persist"
#define TEST_PREFIX "test_state"
#define TEST_VALUE "test_value"

typedef struct _TestState
{
  PersistableStateHeader super;
  gchar value[15];
} TestState;

gboolean
test_state_dump_state(PersistableStateHeader *super, PresentedPersistableState *result)
{
  TestState *state = (TestState*) super;
  presented_persistable_state_add_int(result, "version", state->super.version);
  presented_persistable_state_add_boolean(result, "big_endian",
      state->super.big_endian);
  presented_persistable_state_add_string(result, "value", state->value);
  return TRUE;
}

gboolean
test_state_load_state(PersistableStateHeader *super, PresentedPersistableState *result)
{
  TestState *state = (TestState*) super;
  state->super.version = presented_persistable_state_get_int(result, "version");
  state->super.big_endian = presented_persistable_state_get_boolean(result, "big_endian");
  strncpy(&state->value, presented_persistable_state_get_string(result, "value"), 15);
  return TRUE;
}

PersistEntryHandle
test_state_alloc_value(PersistState* state, gchar* name)
{
   guint32 size = sizeof(TestState);
   return persist_state_alloc_entry(state, name, size);
};

PersistableStatePresenter *
create_test_state_handler()
{
  PersistableStatePresenter *self = g_new0(PersistableStatePresenter, 1);
  self->dump = test_state_dump_state;
  self->load = test_state_load_state;
  self->alloc = test_state_alloc_value;
  return self;
}

void
test_state_dump()
{
  TestState *test_state;
  PersistableStatePresenter *state_presenter;
  PresentedPersistableState *dumped_value;
  gchar *value;
  gint version;
  gboolean big_endian;
  PersistableStatePresenterConstructFunc constructor;

  persistable_state_presenter_register_constructor(TEST_PREFIX, create_test_state_handler);

  constructor = persistable_state_presenter_get_constructor_by_prefix(TEST_PREFIX);
  assert_true(constructor != NULL, "Can't get constructor");

  state_presenter = constructor(TEST_PREFIX);
  test_state = g_new0(TestState, 1);

  memset(test_state, 0, sizeof(TestState));
  test_state->super.big_endian = FALSE;
  test_state->super.version = 1;
  memcpy(test_state->value, TEST_VALUE, strlen(TEST_VALUE));

  dumped_value = json_presented_persistable_state_new();
  persistable_state_presenter_dump(state_presenter, (PersistableStateHeader*)test_state, dumped_value);

  version = presented_persistable_state_get_int(dumped_value, "version");
  assert_gint(version, 1, "Bad value");

  big_endian = presented_persistable_state_get_boolean(dumped_value, "big_endian");
  assert_false(big_endian, "Bad value");

  value = presented_persistable_state_get_string(dumped_value, "value");
  assert_string(value, TEST_VALUE, "Bad value");
  presented_persistable_state_free(dumped_value);

  g_free(test_state);
  g_free(state_presenter);

}

#define JSON_STRING "{ \"test_int64\": -4822678189205112, \"test_int\": 2003195204, \"test_boolean\": true, \"test_string\": \"test_value\"}"
#define JSON_STRING_FOR_LOAD "{ \"version\": 1, \"big_endian\": false, \"value\": \"test_value\" }"

void
test_state_load()
{
  PresentedPersistableState *container = json_presented_persistable_state_new();
  json_presented_persistable_state_parse_json_string(container, JSON_STRING_FOR_LOAD);
  PersistableStatePresenter *presenter = create_test_state_handler();
  TestState *state = g_new0(TestState, 1);

  persistable_state_presenter_load(presenter, state, container);

  assert_gint(state->super.version, 1, "Version is not loaded correctly!");
  assert_false(state->super.big_endian, "Endianness is not loaded correctly!");
  assert_string(&state->value, "test_value", "Value is not loaded correctly!");

  g_free(state);
  presented_persistable_state_free(container);
  g_free(presenter);
}

void
test_presented_persistable_state()
{
  PresentedPersistableState *container = json_presented_persistable_state_new();
  gint64 value_int64 = 0xFFEEDDCCBBAA9988;
  gint value_int = 0x77665544;
  gchar *value_string = "test_value";
  gboolean value_boolean = TRUE;

  presented_persistable_state_add_string(container, "test_string", value_string);
  presented_persistable_state_add_int64(container, "test_int64", value_int64);
  presented_persistable_state_add_int(container, "test_int", value_int);
  presented_persistable_state_add_boolean(container, "test_boolean", value_boolean);

  assert_gint64(presented_persistable_state_get_int64(container, "test_int64"), value_int64, "Bad int64 behavior");
  assert_gint(presented_persistable_state_get_int(container, "test_int"), value_int, "Bad int behavior");
  assert_gboolean(presented_persistable_state_get_boolean(container, "test_boolean"), value_boolean, "Bad boolean behavior");
  assert_string(presented_persistable_state_get_string(container, "test_string"), value_string, "Bad string behavior");

  presented_persistable_state_free(container);

  container = json_presented_persistable_state_new();

  assert_true(json_presented_persistable_state_parse_json_string(container, JSON_STRING),"Can't parse valid json string");
  assert_gint64(presented_persistable_state_get_int64(container, "test_int64"), value_int64, "Bad int64 behavior");
  assert_gint(presented_persistable_state_get_int(container, "test_int"), value_int, "Bad int behavior");
  assert_gboolean(presented_persistable_state_get_boolean(container, "test_boolean"), value_boolean, "Bad boolean behavior");
  assert_string(presented_persistable_state_get_string(container, "test_string"), value_string, "Bad string behavior");

  presented_persistable_state_free(container);
}

void
test_state_allocation()
{
  gsize size;
  guint8 version;

  PersistState *state = clean_and_create_persist_state_for_test("test_alloc.state");
  PersistableStatePresenter* presenter = create_test_state_handler();

  persistable_state_presenter_alloc(presenter, state, "alma");

  PersistEntryHandle handle = persist_state_lookup_entry(state, "alma", &size, &version);

  assert_gint32(sizeof(TestState), size, "TestState allocated size does not match!");
  assert_true(handle != 0, "Allocated handle should not be null!");

  cancel_and_destroy_persist_state(state);
  g_free(presenter);
}

int
main(int argc, char *argv[])
{
  app_startup();
  test_presented_persistable_state();
  test_state_dump();
  test_state_load();
  test_state_allocation();
  app_shutdown();
  return 0;
}
