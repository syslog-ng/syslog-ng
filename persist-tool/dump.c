/*
 * Copyright (c) 2019 Balabit
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

#include "dump.h"
#include "state.h"

void
print_struct(gpointer data, gpointer user_data)
{
  PersistTool *self = (PersistTool *)user_data;
  gchar *name = (gchar *)data;
  StateHandler *handler;
  NameValueContainer *nv_pairs;

  handler = persist_tool_get_state_handler(self, name);
  nv_pairs = handler->dump_state(handler);

  fprintf(stdout, "%s = %s\n\n", (char *) data, name_value_container_get_json_string(nv_pairs));

  name_value_container_free(nv_pairs);
  state_handler_free(handler);
}

gint
dump_main(int argc, char *argv[])
{
  gint result = 0;
  PersistTool *self;
  GList *keys;

  if (argc < 2)
    {
      fprintf(stderr, "Persist file is a required parameter\n");
      return 1;
    }

  if (!g_file_test(argv[1], G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS))
    {
      fprintf(stderr, "Persist file doesn't exist; file = %s\n", argv[1]);
      return 1;
    }

  self = persist_tool_new(argv[1], persist_mode_dump);
  if (!self)
    {
      return 1;
    }

  keys = persist_state_get_key_list(self->state);

  g_list_foreach(keys, print_struct, self);
  g_list_free(keys);

  persist_tool_free(self);
  return result;
}
