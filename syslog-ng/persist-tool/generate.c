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

#include "generate.h"
#include "driver.h"
#include "apphook.h"
#include <unistd.h>

gint
generate_main(int argc, char *argv[])
{
  gchar *filename;
  PersistTool *self;

  if (!generate_output_dir)
    {
      fprintf(stderr, "output-dir is required options\n");
      return 1;
    }

  if (!g_file_test(generate_output_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
    {
      fprintf(stderr, "Directory doesn't exist: %s\n", generate_output_dir);
      return 1;
    }
  filename = g_build_path(G_DIR_SEPARATOR_S, generate_output_dir, DEFAULT_PERSIST_FILE, NULL);
  if (g_file_test(filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS) && !force_generate)
    {
      fprintf(stderr, "Persist file exists; filename = %s\n", filename);
      return 1;
    }
  else if (g_file_test(filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS))
    {
      if (unlink(filename) != 0)
        {
          fprintf(stderr, "Can't delete existing persist file; file = %s, error = %s\n", filename, strerror(errno));
          return 1;
        }
    }

  self = persist_tool_new(filename, persist_mode_edit);
  if (!self)
    {
      fprintf(stderr, "Error creating persist tool\n");
      return 1;
    }

  fprintf(stderr, "New persist file generated: %s\n", filename);
  persist_state_commit(self->state);

  persist_tool_free(self);
  g_free(filename);

  return 0;
}
