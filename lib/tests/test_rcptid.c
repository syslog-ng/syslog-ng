/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2013 Viktor Juhasz
 * Copyright (c) 2013 Viktor Tusa
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
#include "testutils.h"
#include "syslog-ng.h"
#include "apphook.h"
#include "rcptid.h"
#include "libtest/persist_lib.h"

#include <unistd.h>

gboolean verbose = FALSE;
PersistState *state;

void
setup_persist_id_test()
{
  state = clean_and_create_persist_state_for_test("test_values.persist");
  rcptid_init(state, TRUE);
}

void
teardown_persist_id_test()
{
  commit_and_destroy_persist_state(state);
  rcptid_deinit();
}

static void
test_rcptid_is_persistent_across_persist_backend_reinits(void)
{
  guint64 rcptid;

  setup_persist_id_test();

  rcptid_set_id(0x0000FFFFFFFFFFFE);
  rcptid = rcptid_generate_id();

  assert_guint64(rcptid, 0x0000FFFFFFFFFFFE, "Rcptid initialization to specific value failed!");
  
  state = restart_persist_state(state);

  rcptid_deinit();
  rcptid_init(state, TRUE);

  rcptid = rcptid_generate_id();

  assert_guint64(rcptid, 0x0000FFFFFFFFFFFF, "Rcptid did not persisted across persist backend reinit!");

  teardown_persist_id_test();
}

static void
test_rcptid_overflows_at_48bits_and_is_reset_to_one(void)
{
  guint64 rcptid;
  
  setup_persist_id_test();

  rcptid_set_id(0x0000FFFFFFFFFFFF);
  rcptid = rcptid_generate_id();
  rcptid = rcptid_generate_id();

  assert_guint64(rcptid, (guint64) 1, "Rcptid counter overflow handling did not work!");
  teardown_persist_id_test();
}

static void
test_rcptid_is_formatted_as_a_number_when_nonzero(void)
{
  GString *formatted_rcptid = g_string_sized_new(0);

  setup_persist_id_test();

  rcptid_append_formatted_id(formatted_rcptid, 1);
  assert_string(formatted_rcptid->str, "1", "RCPTID macro formatting for non-zero number failed!");

  teardown_persist_id_test();
  g_string_free(formatted_rcptid, TRUE);
}

static void
test_rcptid_is_an_empty_string_when_zero(void)
{
  GString *formatted_rcptid = g_string_sized_new(0);

  setup_persist_id_test();

  rcptid_append_formatted_id(formatted_rcptid, 0);
  assert_string(formatted_rcptid->str, "", "RCPTID macro formatting for zero value failed!");

  teardown_persist_id_test();
  g_string_free(formatted_rcptid, TRUE);
}

static void
rcptid_test_case()
{
  test_rcptid_is_persistent_across_persist_backend_reinits();
  test_rcptid_overflows_at_48bits_and_is_reset_to_one();
  test_rcptid_is_formatted_as_a_number_when_nonzero();
  test_rcptid_is_an_empty_string_when_zero();
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  if (argc > 1)
    verbose = TRUE;
  app_startup();

  rcptid_test_case();
  return 0;
}
