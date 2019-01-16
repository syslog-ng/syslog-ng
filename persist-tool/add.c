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
#include "jsonc/json.h"
#include "mainloop.h"
#include "state.h"

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

gboolean
check_file_exists(gchar *path)
{
  return g_file_test(path, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS);
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

void
persist_state_entry_free(PersistStateEntry *self)
{
  g_free(self->name);
  g_free(self->value);
  g_free(self);
}

#define NAME_LENGTH 40
#define MAX_LINE_LEN 4000

gint
add_entry_to_persist_file(gchar *entry, PersistTool *self)
{
  PersistStateEntry *persist_entry = persist_state_entry_new();
  NameValueContainer *container = name_value_container_new();
  StateHandler *state_handler = NULL;
  GError *error = NULL;
  gint result = 0;

  if (!persist_state_parse_entry(persist_entry, entry))
    {
      result = 1;
      fprintf(stderr, "%.*s", NAME_LENGTH, entry);
      error = g_error_new(1, 1, "Invalid entry syntax");
      goto exit;
    }
  fprintf(stderr, "%s", persist_entry->name);
  if (!name_value_container_parse_json_string(container, persist_entry->value))
    {
      result = 1;
      error = g_error_new(1, 1, "JSON parsing error");
      goto exit;
    }
  state_handler = persist_tool_get_state_handler(self, persist_entry->name);
  if (!state_handler)
    {
      result = 1;
      error = g_error_new(1, 1, "Unknown prefix");
      goto exit;
    }

  if (!state_handler_load_state(state_handler, container, &error))
    {
      result = 1;
      goto exit;
    }
exit:
  if (result == 0)
    {
      fprintf(stderr, "\tOK\n");
    }
  else
    {
      fprintf(stderr, "\tFAILED (error: %s)\n",error ? error->message : "<UNKNOWN>");
    }
  if (error)
    {
      g_error_free(error);
    }
  if (state_handler)
    {
      state_handler_free(state_handler);
    }

  name_value_container_free(container);
  persist_state_entry_free(persist_entry);
  return result;
}


gint
add_main(int argc, char *argv[])
{
  int result = 0;
  gchar *filename;
  PersistTool *self;
  gchar *line = g_malloc(MAX_LINE_LEN);

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
  filename = g_build_path(G_DIR_SEPARATOR_S, persist_state_dir, DEFAULT_PERSIST_FILE, NULL);

  if (argc < 2)
    {
      fprintf(stderr, "Input missing\n");
      g_free(filename);
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
          g_free(filename);
          return 1;
        }
    }


  self = persist_tool_new(filename, persist_mode_edit);

  while(fgets(line, MAX_LINE_LEN, input_file))
    {
      if (strlen(line) > 3)
        {
          result |= add_entry_to_persist_file(line, self);
        }
    }
  g_free(line);
  persist_tool_free(self);
  if (input_file != stdin)
    {
      fclose(input_file);
    }
  g_free(filename);
  return result;
}
