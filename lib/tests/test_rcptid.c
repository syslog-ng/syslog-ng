/*
 * Copyright (c) 2002-2018 Balabit
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
#include <criterion/criterion.h>
#include "syslog-ng.h"
#include "apphook.h"
#include "rcptid.h"
#include "libtest/persist_lib.h"

#include <unistd.h>

static PersistState *
setup_persist_id_test(const gchar *persist_file)
{
  PersistState *state;

  state = clean_and_create_persist_state_for_test(persist_file);
  rcptid_init(state, TRUE);
  return state;
}

static void
teardown_persist_id_test(PersistState *state)
{
  commit_and_destroy_persist_state(state);
  rcptid_deinit();
}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  app_shutdown();
}

TestSuite(rcptid, .init = setup, .fini = teardown);

Test(rcptid, test_rcptid_is_persistent_across_persist_backend_reinits)
{
  guint64 rcptid;

  PersistState *state = setup_persist_id_test("test_values.backend_reinits");
  rcptid_set_id(0xFFFFFFFFFFFFFFFE);
  rcptid = rcptid_generate_id();
  cr_assert_eq(rcptid, 0xFFFFFFFFFFFFFFFE, "Rcptid initialization to specific value failed!");

  state = restart_persist_state(state);

  rcptid = rcptid_generate_id();
  cr_assert_eq(rcptid, 0xFFFFFFFFFFFFFFFF, "Rcptid did not persisted across persist backend reinit!");
  teardown_persist_id_test(state);
}

Test(rcptid, test_rcptid_overflows_at_64bits_and_is_reset_to_one)
{
  guint64 rcptid;

  PersistState *state = setup_persist_id_test("test_values.reset_to_one");
  rcptid_set_id(0xFFFFFFFFFFFFFFFF);
  rcptid = rcptid_generate_id();
  rcptid = rcptid_generate_id();
  cr_assert_eq(rcptid, (guint64) 1, "Rcptid counter overflow handling did not work!");
  teardown_persist_id_test(state);
}

Test(rcptid, test_rcptid_is_formatted_as_a_number_when_nonzero)
{
  PersistState *state = setup_persist_id_test("test_values.nonzero");
  GString *formatted_rcptid = g_string_sized_new(0);
  rcptid_append_formatted_id(formatted_rcptid, 1);
  cr_assert_str_eq(formatted_rcptid->str, "1", "RCPTID macro formatting for non-zero number failed!");
  g_string_free(formatted_rcptid, TRUE);
  teardown_persist_id_test(state);
}

Test(rcptid, test_rcptid_is_an_empty_string_when_zero)
{
  PersistState *state = setup_persist_id_test("test_values.zero");
  GString *formatted_rcptid = g_string_sized_new(0);
  rcptid_append_formatted_id(formatted_rcptid, 0);
  cr_assert_str_eq(formatted_rcptid->str, "", "RCPTID macro formatting for zero value failed!");
  g_string_free(formatted_rcptid, TRUE);
  teardown_persist_id_test(state);
}
