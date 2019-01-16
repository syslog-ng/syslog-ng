/*
 * Copyright (c) 2019 Balabit
 * Copyright (c) 1998-2010 Balázs Scheidler
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
#include "add.h"
#include "generate.h"
#include "state.h"
#include "persist-tool.h"
#include "reloc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if HAVE_GETOPT_H
#include <getopt.h>
#endif

static void
load_state_handler_modules(GlobalConfig *cfg)
{
  plugin_context_init_instance(&cfg->plugin_context);
  plugin_context_set_module_path(&cfg->plugin_context, (const gchar *)get_installation_path_for(SYSLOG_NG_MODULE_PATH));
  plugin_load_candidate_modules(&cfg->plugin_context);
}

static gboolean
persist_tool_start_state(PersistTool *self)
{
  if (!g_threads_got_initialized)
    {
      g_thread_init(NULL);
    }
  main_thread_handle = get_thread_id();
  self->state = persist_state_new(self->persist_filename);
  if (!persist_state_start(self->state))
    {
      fprintf(stderr, "Invalid persist file: %s\n", self->persist_filename);
      persist_state_cancel(self->state);
      persist_state_free(self->state);
      self->state = NULL;
      return FALSE;
    }
  return TRUE;
}

void
persist_tool_revert_changes(PersistTool *self)
{
  if (self->state == NULL)
    {
      self->state = self->cfg->state;
    }
  persist_state_cancel(self->state);
  persist_state_start(self->state);
}

PersistTool *
persist_tool_new(gchar *persist_filename, PersistStateMode open_mode)
{
  PersistTool *self = g_new0(PersistTool, 1);
  self->mode = open_mode;
  self->persist_filename = g_strdup(persist_filename);
  self->cfg = cfg_new(0);
  if (!persist_tool_start_state(self))
    {
      persist_tool_free(self);
      return NULL;
    }
  state_handler_register_default_constructors();
  load_state_handler_modules(self->cfg);
  return self;
}

void persist_tool_free(PersistTool *self)
{
  if (self->cfg)
    {
      cfg_free(self->cfg);
    }
  if (self->state)
    {
      if (self->mode != persist_mode_dump)
        {
          persist_state_commit(self->state);
        }
      else
        {
          persist_state_cancel(self->state);
        }
      persist_state_free(self->state);
    }
  g_free(self->persist_filename);
  g_free(self);
}

StateHandler *persist_tool_get_state_handler(PersistTool *self, gchar *name)
{
  STATE_HANDLER_CONSTRUCTOR constructor = NULL;
  StateHandler *handler = NULL;
  gchar *prefix;
  gchar *p;

  p = strchr(name, '(');
  if (!p)
    {
      p = strchr(name, ',');
    }

  if (p)
    {
      prefix = g_strndup(name, p - name);
    }
  else
    {
      prefix = g_strdup(name);
    }
  constructor = state_handler_get_constructor_by_prefix(prefix);
  if (!constructor)
    {
      return NULL;
    }
  handler = constructor(self->state, name);
  return handler;
}

static GOptionEntry dump_options[] =
{
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static GOptionEntry add_options[] =
{
  { "output-dir", 'o', 0, G_OPTION_ARG_STRING, &persist_state_dir,"The directory where persist file is located. The name of the persist file stored in this directory must be syslog-ng.persist", "<directory>" },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

static GOptionEntry generate_options[] =
{
  { "force", 'f', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_NONE, &force_generate, "Overwrite the persist file if it already exists. WARNING: Use this option with care, persist-tool will not ask for confirmation", NULL},
  { "config-file", 'c', 0, G_OPTION_ARG_FILENAME, &config_file_generate, "The syslog-ng configuration file that will be the base of the generated persist", "<config_file>" },
  { "output-dir", 'o', 0, G_OPTION_ARG_FILENAME, &generate_output_dir, "The directory where persist file will be generated to. The name of the persist file will be syslog-ng.persist", "<directory>"},
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL }
};

typedef struct _PersistToolMode
{
  const gchar *mode;
  const GOptionEntry *options;
  const gchar *description;
  const gchar *parameter_description;
  gint
  (*main)(gint argc, gchar *argv[]);
} PersistToolMode;

static PersistToolMode modes[] =
{
  { "dump", dump_options, "Dump the contents of the persist file", "<persist_file>", dump_main },
  { "generate", generate_options, "Generate a persist file from the configuration file of syslog-ng", "", generate_main },
  { "add", add_options, "Add new or change entry in the persist file", "<input_file>", add_main },
  { NULL, NULL },
};

const gchar *
get_mode(int *argc, char **argv[])
{
  gint i;
  const gchar *mode;

  for (i = 1; i < (*argc); i++)
    {
      if ((*argv)[i][0] != '-')
        {
          mode = (*argv)[i];
          memmove(&(*argv)[i], &(*argv)[i+1], ((*argc) - i) * sizeof(gchar *));
          (*argc)--;
          return mode;
        }
    }
  return NULL;
}

void
usage(const gchar *bin_name)
{
  gint mode;

  fprintf(stderr, "Syntax: %s <command> [options]\nPossible commands are:\n",
          bin_name);
  for (mode = 0; modes[mode].mode; mode++)
    {
      fprintf(stderr, "    %-20s %s\n", modes[mode].mode,
              modes[mode].description);
    }
}

int
main(int argc, char *argv[])
{
  const gchar *mode_string;
  GOptionContext *ctx;
  gint mode;
  GError *error = NULL;
  int result;

  setvbuf(stderr, NULL, _IONBF, 0);
  mode_string = get_mode(&argc, &argv);
  if (!mode_string)
    {
      usage(argv[0]);
      return 1;
    }

  ctx = NULL;

  for (mode = 0; modes[mode].mode; mode++)
    {
      if (strcmp(modes[mode].mode, mode_string) == 0)
        {
          ctx = g_option_context_new(modes[mode].parameter_description);
#if GLIB_CHECK_VERSION (2, 12, 0)
          g_option_context_set_summary(ctx, modes[mode].description);
#endif
          g_option_context_add_main_entries(ctx, modes[mode].options, NULL );
          break;
        }
    }
  if (!ctx)
    {
      fprintf(stderr, "Unknown command\n");
      usage(argv[0]);
      return 1;
    }

  argv[0] = g_strdup_printf("%s %s", argv[0], mode_string);

  if (!g_option_context_parse(ctx, &argc, &argv, &error))
    {
      fprintf(stderr, "Error parsing command line arguments: %s\n",
              error ? error->message : "Invalid arguments");
      g_clear_error(&error);
      g_option_context_free(ctx);
      return 1;
    }
  g_option_context_free(ctx);

  result = modes[mode].main(argc, argv);
  return result;
}
