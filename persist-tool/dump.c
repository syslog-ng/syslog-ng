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

#include "dump.h"
#include <json.h>
#include "json-presented-persistable-state.h"
#include "str-format.h"
#include "lib/pathutils.h"

void
print_raw_struct_in_hex(gchar* name, gint size, gpointer entry)
{
  gchar* hex_converted_raw_data = g_malloc(size * 3);
  format_hex_string_with_delimiter(entry, size, hex_converted_raw_data, size * 3, ' ');
  fprintf(stdout, "%s = hex(%s)\n\n", name, hex_converted_raw_data);
  g_free(hex_converted_raw_data);
};

void
print_struct(gchar* name, gint size, gpointer entry, gpointer userdata)
{
  PersistTool *self = (PersistTool *)userdata;
  PersistableStatePresenter *presenter;
  PresentedPersistableState *representation;

  PersistableStateHeader* state = (PersistableStateHeader*) entry;
  presenter = persist_tool_get_state_presenter(self, name);

  if (!presenter)
    {
      print_raw_struct_in_hex(name, size, entry);
      return;
    }

  representation = json_presented_persistable_state_new();
  persistable_state_presenter_dump(presenter, state, representation);

  fprintf(stdout, "%s = %s\n\n", name, json_presented_persistable_state_get_json_string((JsonPresentedPersistableState*)representation));

  presented_persistable_state_free(representation);
  g_free(presenter);

};

gint
dump_main(int argc, char *argv[])
{
  gint result = 0;
  PersistTool *self;

  if (argc < 2)
    {
      fprintf(stderr, "Persist file is a required parameter\n");
      return 1;
    }

  if (!is_file_regular(argv[1]))
    {
      fprintf(stderr, "Persist file doesn't exist; file = %s\n", argv[1]);
      return 1;
    }

  self = persist_tool_new(argv[1], persist_mode_dump);
  if (!self)
    {
      return 1;
    }

  persist_state_foreach_entry(self->state, print_struct, self);

  persist_tool_free(self);
  return result;
}
