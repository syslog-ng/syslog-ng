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

#ifndef GENERATE_H
#define GENERATE_H 1

#include "persist-tool.h"
#include "syslog-ng.h"
#include "mainloop.h"
#include "persist-state.h"
#include "plugin.h"
#include "persistable-state-presenter.h"
#include "cfg.h"

gboolean force_generate;
gchar *config_file_generate;
gchar *generate_output_dir;

static GOptionEntry generate_options[] =
{
  { "force", 'f', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_NONE, &force_generate, "Overwrite the persist file if it already exists. WARNING: Use this option with care, persist-tool will not ask for confirmation", NULL},
  { "config-file", 'c', 0, G_OPTION_ARG_FILENAME, &config_file_generate, "The syslog-ng configuration file that will be the base of the generated persist", "<config_file>" },
  { "output-dir", 'o', 0, G_OPTION_ARG_FILENAME, &generate_output_dir, "The directory where persist file will be generated to. The name of the persist file will be syslog-ng.persist", "<directory>"},
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

gint generate_main(int argc, char *argv[]);

#endif
