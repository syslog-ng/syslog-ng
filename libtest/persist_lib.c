/*
 * Copyright (c) 2014 Balabit
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

#include <persist_lib.h>
#include <stdlib.h>
#include <unistd.h>

PersistState *
create_persist_state_for_test(const gchar *name)
{
  PersistState *state = persist_state_new(name);
  if (!persist_state_start(state))
    {
      fprintf(stderr, "Error starting persist_state object\n");
      exit(1);
    }

  return state;
};

PersistState *
clean_and_create_persist_state_for_test(const gchar *name)
{
  unlink(name);

  return create_persist_state_for_test(name);
};

PersistState *
restart_persist_state(PersistState *state)
{
  PersistState *new_state;

  gchar *name = g_strdup(persist_state_get_filename(state));

  persist_state_commit(state);
  persist_state_free(state);

  new_state = create_persist_state_for_test(name);

  g_free(name);
  return new_state;
};

void
cancel_and_destroy_persist_state(PersistState *state)
{
  gchar *filename = g_strdup(persist_state_get_filename(state));

  persist_state_cancel(state);
  persist_state_free(state);

  unlink(filename);
  g_free(filename);
};

void
commit_and_free_persist_state(PersistState *state)
{
  persist_state_commit(state);
  persist_state_free(state);
};

void
commit_and_destroy_persist_state(PersistState *state)
{
  gchar *filename = g_strdup(persist_state_get_filename(state));

  commit_and_free_persist_state(state);

  unlink(filename);
  g_free(filename);
};
