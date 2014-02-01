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

#include "generate.h"
#include "generate-common.h"
#include "driver.h"
#include "apphook.h"
#include "lib/pathutils.h"

static gboolean
_config_load_callback(GlobalConfig* config, gchar* config_file)
{
  return cfg_read_config(config, config_file, FALSE, NULL);
};

gint
generate_main(int argc, char *argv[])
{
  gchar *filename;
  PersistTool *self;

  if (!generate_output_dir || !config_file_generate)
    {
      fprintf(stderr,"config-file and output-dir are required options\n");
      return 1;
    }

  if (!validate_filename_and_output_directory(generate_output_dir, &filename, force_generate))
    return 1;

  if (!is_file_regular(config_file_generate))
    {
      fprintf(stderr, "Given config file doesn't exists; filename = %s\n", config_file_generate);
      return 1;
    }

  generate_command_main(filename, config_file_generate, _config_load_callback);

  g_free(filename);
  return 0;
}
