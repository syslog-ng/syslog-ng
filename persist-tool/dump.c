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

static void
print_struct_json_style(gpointer data, gpointer user_data)
{
  PersistEntryHandle handle;
  gsize size;
  guint8 result_version;

  PersistTool *self = (PersistTool *)user_data;
  gchar *name = (gchar *)data;

  if (!(handle = persist_state_lookup_entry(self->state, name, &size, &result_version)))
    {
      fprintf(stderr,"Can't lookup for entry \"%s\"\n", name);
      return;
    }

  gpointer block = persist_state_map_entry(self->state, handle);

  fprintf(stdout,"\n%s = { \"value\": \"", name);
  gchar *block_data = (gchar *) block;
  for (gsize i=0; i<size; i++)
    {
      fprintf(stdout, "%.2X ", block_data[i]&0xff);
    }
  fprintf(stdout,"\" }\n");

  persist_state_unmap_entry(self->state, handle);
}

gint
dump_main(int argc, char *argv[])
{
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

  PersistTool *self = persist_tool_new(argv[1], persist_mode_dump);
  if (!self)
    {
      fprintf(stderr,"Error creating persist tool\n");
      return 1;
    }

  GList *keys = g_hash_table_get_keys(self->state->keys);

  g_list_foreach(keys, print_struct_json_style, self);
  g_list_free(keys);

  persist_tool_free(self);
  return 0;
}
