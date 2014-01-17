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

#include "syslog-ng.h"
#include "lib/pathutils.h"
#include <stdio.h>
#include "generate-common.h"
#include "apphook.h"

gboolean
validate_filename_and_output_directory(const char *directory, char **generated_filename, gboolean force_generate)
{
   char *filename;

   if (!is_file_directory(directory))
    {
      fprintf(stderr, "Directory doesn't exist: %s\n", directory);
      return FALSE;
    }

  filename = g_build_path(G_DIR_SEPARATOR_S, directory, DEFAULT_PERSIST_FILE, NULL);
  if (is_file_regular(filename))
    {
      if (!force_generate)
        {
          fprintf(stderr, "Persist file exists; filename = %s\n", filename);
          g_free(filename);
          return FALSE;
        }

      if (unlink(filename) != 0)
        {
          fprintf(stderr, "Can't delete existing persist file; file = %s, error = %s\n", filename, strerror(errno));
          g_free(filename);
          return FALSE;
        }
    }

  *generated_filename = filename;
  return TRUE;
};

void
generate_command_main(const char *filename, const char *config, gboolean (*config_load_function)(GlobalConfig *config, gchar *config_string))
{
  PersistTool *self;

  app_startup();
  self = persist_tool_new(filename, persist_mode_edit);

  if (config_load_function(self->cfg, config))
    {
      self->cfg->state = self->state;
      self->state = NULL;
      cfg_generate_persist_file(self->cfg->state);
      fprintf(stderr,"New persist file generated: %s\n", filename);
      persist_state_commit(self->cfg->state);
    }
  persist_tool_free(self);
  app_shutdown();

};
