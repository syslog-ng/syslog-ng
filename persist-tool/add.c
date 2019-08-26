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

#include "add.h"

typedef struct __PersistStateEntry
{
  gchar *name;
  gchar *value;
} PersistStateEntry;

FILE *input_file;

gboolean
check_directory_exists(gchar *path)
{
  return g_file_test(path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR);
}

PersistStateEntry *
persist_state_entry_new(void)
{
  PersistStateEntry *self = g_new0(PersistStateEntry, 1);
  return self;
}

gboolean
persist_state_parse_entry(PersistStateEntry *self, gchar *entry)
{
  gchar *p = strchr(entry, '=');
  if (!p)
    {
      return FALSE;
    }
  self->name = g_strndup(entry, p - entry);
  self->name = g_strstrip(self->name);
  self->value = g_strdup((p+1));
  return TRUE;
}

#define NAME_LENGTH 40
#define MAX_LINE_LEN 4000
#define MAX_ENTRY_VALUE_LENGTH (3*MAX_LINE_LEN)

/* This function parses a JSON style string
 * containing series of integer values.
 * For example:
 * { "value": "00 00 00 00 01 02 03 0A" } */
gint
parse_value_string(gchar *value_str, gchar *buffer, gint buffer_length)
{
  gchar *p = strchr(value_str, ':');
  if (!p)
    return -1;

  p = strchr(p, '"');
  if (!p || strlen(p)<1)
    return -1;

  p = g_strchug(p+1); // p points to the first value

  gint count = 0;
  gint token_index = 0;
  gchar *token = NULL;

  gchar **token_list = g_strsplit (p," ",-1);
  while ((token = token_list[token_index++]))
    {
      int value;
      int result = sscanf(token, "%X", &value);

      if (result == EOF || result == 0)
        continue;

      if (count >= buffer_length)
        break;

      if (value > 0xFF)
        {
          fprintf(stderr,"invalid numeric value (%s)\n", token);
          return -1;
        }

      buffer[count++] = (gchar)value;
    }

  g_strfreev(token_list);
  return count;
}

void
persist_state_entry_free(PersistStateEntry *self)
{
  g_free(self->name);
  g_free(self->value);
  g_free(self);
}

gint
add_entry_to_persist_file(gchar *entry, PersistTool *self)
{
  PersistStateEntry *persist_entry = persist_state_entry_new();
  GError *error = NULL;
  gint result = 0;

  if (!persist_state_parse_entry(persist_entry, entry))
    {
      result = 1;
      fprintf(stderr, "%.*s", NAME_LENGTH, entry);
      error = g_error_new(1, 1, "Invalid entry syntax");
      goto exit;
    }

  gchar buffer[MAX_ENTRY_VALUE_LENGTH];
  gint value_count = parse_value_string(persist_entry->value, buffer, MAX_ENTRY_VALUE_LENGTH);
  if (value_count < 0)
    {
      result = 1;
      fprintf(stderr,"value string is invalid (%s)\n", entry);
      error = g_error_new(1, 1, "Invalid value string in input");
      goto exit;
    }

  PersistEntryHandle handle = persist_state_alloc_entry(self->state, persist_entry->name, value_count);
  if (!handle)
    {
      result = 1;
      fprintf(stderr, "%.*s", NAME_LENGTH, entry);
      error = g_error_new(1, 1, "Can't alloc entry");
      goto exit;
    }

  gpointer block = persist_state_map_entry(self->state, handle);
  memcpy(block, buffer, value_count);

  persist_state_unmap_entry(self->state, handle);

exit:
  if (result == 0)
    {
      fprintf(stderr, "%s\t-->>OK\n", persist_entry->name);
    }
  else
    {
      fprintf(stderr, "%s\t-->>FAILED (error: %s)\n", persist_entry->name, error ? error->message : "<UNKNOWN>");
    }
  g_clear_error(&error);

  persist_state_entry_free(persist_entry);
  return result;
}

static void
lookup_entry(gpointer data, gpointer user_data)
{

  PersistTool *self = (PersistTool *)user_data;
  gchar *name = (gchar *)data;

  gsize value_len = 0;
  guint8 version = 0;
  persist_state_lookup_entry(self->state, name, &value_len, &version);
}

/* this function reads all entries in persist to update the in_use struct member */
static void
ref_all_entries(PersistTool *self)
{
  GList *keys = g_hash_table_get_keys(self->state->keys);
  g_list_foreach(keys, lookup_entry, self);
  g_list_free(keys);
}

gint
add_main(int argc, char *argv[])
{
  int result = 0;

  if (!persist_state_dir)
    {
      fprintf(stderr,"Required option (output-dir) is missing\n");
      return 1;
    }

  if (!check_directory_exists(persist_state_dir))
    {
      fprintf(stderr, "Directory doesn't exist: %s\n", persist_state_dir);
      return 1;
    }

  if (argc < 2)
    {
      fprintf(stderr, "Input missing\n");
      return 1;
    }

  if (strcmp(argv[1], "-") == 0)
    {
      input_file = stdin;
      fprintf(stdout, "Escape character is '^d'\n");
    }
  else
    {
      input_file = fopen(argv[1], "r");
      if (input_file == NULL)
        {
          fprintf(stderr, "Can't open input file; file = \"%s\", error = %s\n", argv[1], strerror(errno));
          return 1;
        }
    }

  gchar *filename = g_build_path(G_DIR_SEPARATOR_S, persist_state_dir,
                                 persist_state_name ? persist_state_name : DEFAULT_PERSIST_FILE, NULL);


  PersistTool *self = persist_tool_new(filename, persist_mode_edit);
  if (!self)
    {
      fprintf(stderr,"Error creating persist tool\n");
      g_free(filename);
      return 1;
    }

  gchar *line = g_malloc(MAX_LINE_LEN);
  while(fgets(line, MAX_LINE_LEN, input_file))
    {
      if (strlen(line) > 3)
        {
          result |= add_entry_to_persist_file(line, self);
        }
    }
  g_free(line);

  ref_all_entries(self);

  persist_tool_free(self);
  if (input_file != stdin)
    {
      fclose(input_file);
    }
  g_free(filename);
  return result;
}
