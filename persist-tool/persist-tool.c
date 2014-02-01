/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
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
#include "persist-tool.h"
#include "mainloop.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "logproto/logproto-buffered-server.h"
#include "lib/reloc.h"
#include "lib/messages.h"

#if HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifndef _WIN32
#define STATE_HANDLER_MODULES "afsqlsource,basic-proto,rltp-proto,disk-buffer,eventlog"
#else
#define STATE_HANDLER_MODULES "basic-proto,rltp-proto,disk-buffer,eventlog"
#endif

static void
load_state_handler_modules(GlobalConfig *cfg)
{
  gint i;
  gchar **mods;
  mods = g_strsplit(STATE_HANDLER_MODULES, ",", 0);
  for (i = 0; mods[i]; i++)
    {
      plugin_load_module(mods[i], cfg, NULL );
    }
  g_strfreev(mods);
}

static gboolean
persist_tool_start_state(PersistTool *self)
{
  gboolean result = FALSE;
  if (!g_threads_got_initialized)
    {
      g_thread_init(NULL);
    }
  main_thread_handle = get_thread_id();
  self->state = persist_state_new(self->persist_filename);
  switch (self->mode)
    {
    case persist_mode_normal:
      result = persist_state_start(self->state);
      break;
    case persist_mode_edit:
      result = persist_state_start_edit(self->state);
      break;
    case persist_mode_dump:
      result = persist_state_start_dump(self->state);
      break;
    }
  if (!result)
    {
      fprintf(stderr, "Invalid persist file: %s\n", self->persist_filename);
      persist_state_cancel(self->state);
      persist_state_free(self->state);
      self->state = NULL;
      return FALSE;
    }
  return TRUE;
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

PersistableStatePresenter *persist_tool_get_state_presenter(PersistTool *self, gchar *name)
{
  PersistableStatePresenterConstructFunc constructor = NULL;
  PersistableStatePresenter *handler = NULL;
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
  constructor = persistable_state_presenter_get_constructor_by_prefix(prefix);
  if (!constructor)
    {
      return NULL;
    }
  handler = constructor(name);
  return handler;
}

typedef struct _PersistToolCommand
{
  const gchar *command_name;
  const GOptionEntry *options;
  const gchar *description;
  const gchar *parameter_description;
  gint
  (*main)(gint argc, gchar *argv[]);
} PersistToolCommand;

static PersistToolCommand commands[] =
{
  { "dump", dump_options, "Dump the contents of the persist file", "<persist_file>", dump_main },
  { "generate", generate_options, "Generate a persist file from the configuration file of syslog-ng", "", generate_main },
  { "add", add_options, "Add new or change entry in the persist file", "<input_file>", add_main },
  { NULL, NULL },
};

const gchar *
get_command(int *argc, char **argv[])
{
  gint i;
  const gchar *command;

  for (i = 1; i < (*argc); i++)
    {
      if ((*argv)[i][0] != '-')
        {
          command = (*argv)[i];
          memmove(&(*argv)[i], &(*argv)[i+1], ((*argc) - i) * sizeof(gchar *));
          (*argc)--;
          return command;
        }
    }
  return NULL;
}

void
usage(const gchar *bin_name)
{
  gint command;

  fprintf(stderr, "Syntax: %s <command> [options]\nPossible commands are:\n", bin_name);

  for (command = 0; commands[command].command_name; command++)
    {
      fprintf(stderr, "    %-20s %s\n", commands[command].command_name,
              commands[command].description);
    }
  exit(1);
}

int
main(int argc, char *argv[])
{
  const gchar *command_string;
  GOptionContext *ctx;
  gint command;
  GError *error = NULL;
  int result;

  setvbuf(stderr, NULL, _IONBF, 0);
  command_string = get_command(&argc, &argv);
  if (!command_string)
    {
      usage(argv[0]);
    }

  ctx = NULL;

  for (command = 0; commands[command].command_name; command++)
    {
      if (strcmp(commands[command].command_name, command_string) == 0)
        {
          ctx = g_option_context_new(commands[command].parameter_description);
#if GLIB_CHECK_VERSION (2, 12, 0)
          g_option_context_set_summary(ctx, commands[command].description);
#endif
          g_option_context_add_main_entries(ctx, commands[command].options, NULL );
          break;
        }
    }
  if (!ctx)
    {
      fprintf(stderr, "Unknown command\n");
      usage(argv[0]);
    }

  argv[0] = g_strdup_printf("%s %s", argv[0], command_string);

  if (!g_option_context_parse(ctx, &argc, &argv, &error))
    {
      fprintf(stderr, "Error parsing command line arguments: %s\n",
              error ? error->message : "Invalid arguments");
      g_clear_error(&error);
      g_option_context_free(ctx);
      return 1;
    }
  g_option_context_free(ctx);
  module_path = get_installation_path_for(MODULE_PATH);
  msg_init(TRUE);

  result = commands[command].main(argc, argv);
  return result;
}
