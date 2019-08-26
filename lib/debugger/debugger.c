/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balázs Scheidler
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
#include "debugger/debugger.h"
#include "debugger/tracer.h"
#include "logmsg/logmsg.h"
#include "logpipe.h"
#include "apphook.h"
#include "mainloop.h"

#include <stdio.h>
#include <unistd.h>

struct _Debugger
{
  Tracer *tracer;
  MainLoop *main_loop;
  GlobalConfig *cfg;
  gchar *command_buffer;
  LogTemplate *display_template;
  LogMessage *current_msg;
  LogPipe *current_pipe;
  gboolean drop_current_message;
};

static gboolean
_format_nvpair(NVHandle handle, const gchar *name, const gchar *value, gssize length, gpointer user_data)
{
  printf("%s=%.*s\n", name, (gint) length, value);
  return FALSE;
}

static void
_display_msg_details(Debugger *self, LogMessage *msg)
{
  GString *output = g_string_sized_new(128);

  log_msg_values_foreach(msg, _format_nvpair, NULL);
  g_string_truncate(output, 0);
  log_msg_print_tags(msg, output);
  printf("TAGS=%s\n", output->str);
  printf("\n");
  g_string_free(output, TRUE);
}

static void
_display_msg_with_template(Debugger *self, LogMessage *msg, LogTemplate *template)
{
  GString *output = g_string_sized_new(128);

  log_template_format(template, msg, NULL, LTZ_LOCAL, 0, NULL, output);
  printf("%s\n", output->str);
  g_string_free(output, TRUE);
}

static gboolean
_display_msg_with_template_string(Debugger *self, LogMessage *msg, const gchar *template_string, GError **error)
{
  LogTemplate *template;

  template = log_template_new(self->cfg, NULL);
  if (!log_template_compile(template, template_string, error))
    {
      return FALSE;
    }
  _display_msg_with_template(self, msg, template);
  log_template_unref(template);
  return TRUE;
}

static void
_display_source_line(LogExprNode *expr_node)
{
  FILE *f;
  gint lineno = 1;
  gchar buf[1024];

  if (!expr_node || !expr_node->filename)
    return;

  f = fopen(expr_node->filename, "r");
  if (f)
    {
      while (fgets(buf, sizeof(buf), f) && lineno < expr_node->line)
        lineno++;
      if (lineno != expr_node->line)
        buf[0] = 0;
      fclose(f);
    }
  else
    {
      buf[0] = 0;
    }
  printf("%-8d %s", expr_node->line, buf);
  if (buf[0] == 0 || buf[strlen(buf) - 1] != '\n')
    putc('\n', stdout);
  fflush(stdout);
}


static gboolean
_cmd_help(Debugger *self, gint argc, gchar *argv[])
{
  printf("syslog-ng interactive console, the following commands are available\n\n"
         "  help, h, or ?            Display this help\n"
         "  continue or c            Continue until the next breakpoint\n"
         "  print, p                 Print the current log message\n"
         "  drop, d                  Drop the current message\n"
         "  quit, q                  Tell syslog-ng to exit\n"
        );
  return TRUE;
}

static gboolean
_cmd_continue(Debugger *self, gint argc, gchar *argv[])
{
  return FALSE;
}

static gboolean
_cmd_print(Debugger *self, gint argc, gchar *argv[])
{
  if (argc == 1)
    _display_msg_details(self, self->current_msg);
  else if (argc == 2)
    {
      GError *error = NULL;
      if (!_display_msg_with_template_string(self, self->current_msg, argv[1], &error))
        {
          printf("print: %s\n", error->message);
          g_clear_error(&error);
        }
    }
  else
    printf("print: expected no arguments or exactly one\n");
  return TRUE;
}

static gboolean
_cmd_display(Debugger *self, gint argc, gchar *argv[])
{
  if (argc == 2)
    {
      GError *error = NULL;
      if (!log_template_compile(self->display_template, argv[1], &error))
        {
          printf("display: Error compiling template: %s\n", error->message);
          g_clear_error(&error);
          return TRUE;
        }
    }
  printf("display: The template is set to: \"%s\"\n", self->display_template->template);
  return TRUE;
}

static gboolean
_cmd_drop(Debugger *self, gint argc, gchar *argv[])
{
  self->drop_current_message = TRUE;
  return FALSE;
}

static gboolean
_cmd_quit(Debugger *self, gint argc, gchar *argv[])
{
  main_loop_exit(self->main_loop);
  self->drop_current_message = TRUE;
  return FALSE;
}

typedef gboolean (*DebuggerCommandFunc)(Debugger *self, gint argc, gchar *argv[]);

struct
{
  const gchar *name;
  DebuggerCommandFunc command;
} command_table[] =
{
  { "help",     _cmd_help },
  { "h",        _cmd_help },
  { "?",        _cmd_help },
  { "continue", _cmd_continue },
  { "c",        _cmd_continue },
  { "print",    _cmd_print },
  { "p",        _cmd_print },
  { "display",  _cmd_display },
  { "drop",     _cmd_drop },
  { "quit",     _cmd_quit },
  { "q",        _cmd_quit },
  { NULL, NULL }
};

gchar *
debugger_builtin_fetch_command(void)
{
  gchar buf[1024];
  gsize len;

  printf("(syslog-ng) ");
  fflush(stdout);

  if (!fgets(buf, sizeof(buf), stdin))
    return NULL;

  /* strip NL */
  len = strlen(buf);
  if (buf[len - 1] == '\n')
    {
      buf[len - 1] = 0;
    }
  return g_strdup(buf);
}

FetchCommandFunc fetch_command_func = debugger_builtin_fetch_command;

void
debugger_register_command_fetcher(FetchCommandFunc fetcher)
{
  fetch_command_func = fetcher;
}

static void
_fetch_command(Debugger *self)
{
  gchar *command;

  command = fetch_command_func();
  if (command && strlen(command) > 0)
    {
      if (self->command_buffer)
        g_free(self->command_buffer);
      self->command_buffer = command;
    }
  else
    {
      if (command)
        g_free(command);
    }
}

static gboolean
_handle_command(Debugger *self)
{
  gint argc;
  gchar **argv;
  GError *error = NULL;
  DebuggerCommandFunc command = NULL;

  if (!g_shell_parse_argv(self->command_buffer ? : "", &argc, &argv, &error))
    {
      printf("%s\n", error->message);
      g_clear_error(&error);
      return TRUE;
    }

  for (gint i = 0; command_table[i].name; i++)
    {
      if (strcmp(command_table[i].name, argv[0]) == 0)
        {
          command = command_table[i].command;
          break;
        }
    }
  if (!command)
    {
      printf("Undefined command %s, try \"help\"\n", argv[0]);
      return TRUE;
    }
  gboolean result = command(self, argc, argv);
  g_strfreev(argv);
  return result;
}

static void
_handle_interactive_prompt(Debugger *self)
{
  gchar buf[1024];
  LogPipe *current_pipe = self->current_pipe;

  printf("Breakpoint hit %s\n", log_expr_node_format_location(current_pipe->expr_node, buf, sizeof(buf)));
  _display_source_line(current_pipe->expr_node);
  _display_msg_with_template(self, self->current_msg, self->display_template);
  while (1)
    {
      _fetch_command(self);

      if (!_handle_command(self))
        break;

    }
  printf("(continuing)\n");
}

static gpointer
_interactive_console_thread_func(Debugger *self)
{
  app_thread_start();
  printf("Waiting for breakpoint...\n");
  while (1)
    {
      tracer_wait_for_breakpoint(self->tracer);

      _handle_interactive_prompt(self);
      tracer_resume_after_breakpoint(self->tracer);
    }
  app_thread_stop();
  return NULL;
}

void
debugger_start_console(Debugger *self)
{
  g_thread_create((GThreadFunc) _interactive_console_thread_func, self, FALSE, NULL);
}

gboolean
debugger_stop_at_breakpoint(Debugger *self, LogPipe *pipe_, LogMessage *msg)
{
  self->drop_current_message = FALSE;
  self->current_msg = log_msg_ref(msg);
  self->current_pipe = log_pipe_ref(pipe_);
  tracer_stop_on_breakpoint(self->tracer);
  log_msg_unref(self->current_msg);
  log_pipe_unref(self->current_pipe);
  self->current_msg = NULL;
  self->current_pipe = NULL;
  return !self->drop_current_message;
}

Debugger *
debugger_new(MainLoop *main_loop, GlobalConfig *cfg)
{
  Debugger *self = g_new0(Debugger, 1);

  self->main_loop = main_loop;
  self->tracer = tracer_new(cfg);
  self->cfg = cfg;
  self->display_template = log_template_new(cfg, NULL);
  self->command_buffer = g_strdup("help");
  log_template_compile(self->display_template, "$DATE $HOST $MSGHDR$MSG", NULL);
  return self;
}

void
debugger_free(Debugger *self)
{
  log_template_unref(self->display_template);
  tracer_free(self->tracer);
  g_free(self->command_buffer);
  g_free(self);
}
