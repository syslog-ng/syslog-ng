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
#include "timeutils/misc.h"
#include "compat/time.h"
#include "scratch-buffers.h"

#include <iv_signal.h>
#include <stdio.h>
#include <unistd.h>

struct _Debugger
{
  Tracer *tracer;
  struct iv_signal sigint;
  MainLoop *main_loop;
  GlobalConfig *cfg;
  gchar *command_buffer;
  LogTemplate *display_template;
  BreakpointSite *breakpoint_site;
  struct timespec last_trace_event;
  GThread *debugger_thread;
};

static gboolean
_format_nvpair(NVHandle handle,
               const gchar *name,
               const gchar *value, gssize length,
               LogMessageValueType type, gpointer user_data)
{
  if (type == LM_VT_BYTES || type == LM_VT_PROTOBUF)
    {
      printf("%s=", name);
      for (gssize i = 0; i < length; i++)
        {
          const guchar b = (const guchar) value[i];
          printf("%02hhx", b);
        }
      printf("\n");
      return FALSE;
    }
  printf("%s=%.*s\n", name, (gint) length, value);
  return FALSE;
}

static void
_display_msg_details(Debugger *self, LogMessage *msg)
{
  GString *output = g_string_sized_new(128);

  log_msg_values_foreach(msg, _format_nvpair, NULL);
  g_string_truncate(output, 0);
  log_msg_format_tags(msg, output, TRUE);
  printf("TAGS=%s\n", output->str);
  printf("\n");
  g_string_free(output, TRUE);
}

static void
_display_msg_with_template(Debugger *self, LogMessage *msg, LogTemplate *template)
{
  GString *output = g_string_sized_new(128);

  log_template_format(template, msg, &DEFAULT_TEMPLATE_EVAL_OPTIONS, output);
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
  if (self->breakpoint_site)
    {
      printf("syslog-ng interactive console\n"
             "Stopped on a breakpoint.\n"
             "The following commands are available:\n\n"
             "  help, h, ?               Display this help\n"
             "  info, i                  Display information about the current execution state\n"
             "  continue, c              Continue until the next breakpoint\n"
             "  display                  Set the displayed message template\n"
             "  trace, t                 Display timing information as the message traverses the config\n"
             "  print, p                 Print the current log message\n"
             "  drop, d                  Drop the current message\n"
             "  quit, q                  Tell syslog-ng to exit\n"
            );
    }
  else
    {
      printf("syslog-ng interactive console\n"
             "Stopped on an interrupt.\n"
             "The following commands are available:\n\n"
             "  help, h, ?               Display this help\n"
             "  continue, c              Continue until the next breakpoint\n"
             "  quit, q                  Tell syslog-ng to exit\n"
            );
    }
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
    _display_msg_details(self, self->breakpoint_site->msg);
  else if (argc == 2)
    {
      GError *error = NULL;
      if (!_display_msg_with_template_string(self, self->breakpoint_site->msg, argv[1], &error))
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
  printf("display: The template is set to: \"%s\"\n", self->display_template->template_str);
  return TRUE;
}

static gboolean
_cmd_drop(Debugger *self, gint argc, gchar *argv[])
{
  self->breakpoint_site->drop = TRUE;
  return FALSE;
}

static gboolean
_cmd_trace(Debugger *self, gint argc, gchar *argv[])
{
  self->breakpoint_site->msg->flags |= LF_STATE_TRACING;
  return FALSE;
}

static gboolean
_cmd_quit(Debugger *self, gint argc, gchar *argv[])
{
  main_loop_exit(self->main_loop);
  if (self->breakpoint_site)
    self->breakpoint_site->drop = TRUE;
  return FALSE;
}

static gboolean
_cmd_info_pipe(Debugger *self, LogPipe *pipe)
{
  gchar buf[1024];

  printf("LogPipe %p at %s\n", pipe, log_expr_node_format_location(pipe->expr_node, buf, sizeof(buf)));
  _display_source_line(pipe->expr_node);

  return TRUE;
}

static gboolean
_cmd_info(Debugger *self, gint argc, gchar *argv[])
{
  if (argc >= 2)
    {
      if (strcmp(argv[1], "pipe") == 0)
        return _cmd_info_pipe(self, self->breakpoint_site->pipe);
    }

  printf("info: List of info subcommands\n"
         "info pipe -- display information about the current pipe\n");
  return TRUE;
}

typedef gboolean (*DebuggerCommandFunc)(Debugger *self, gint argc, gchar *argv[]);

struct
{
  const gchar *name;
  DebuggerCommandFunc command;
  gboolean requires_breakpoint_site;
} command_table[] =
{
  { "help",     _cmd_help },
  { "h",        _cmd_help },
  { "?",        _cmd_help },
  { "continue", _cmd_continue },
  { "c",        _cmd_continue },
  { "print",    _cmd_print, .requires_breakpoint_site = TRUE },
  { "p",        _cmd_print, .requires_breakpoint_site = TRUE },
  { "display",  _cmd_display },
  { "drop",     _cmd_drop, .requires_breakpoint_site = TRUE },
  { "d",        _cmd_drop, .requires_breakpoint_site = TRUE },
  { "quit",     _cmd_quit },
  { "q",        _cmd_quit },
  { "trace",    _cmd_trace, .requires_breakpoint_site = TRUE },
  { "t",        _cmd_trace, .requires_breakpoint_site = TRUE },
  { "info",     _cmd_info, .requires_breakpoint_site = TRUE },
  { "i",        _cmd_info, .requires_breakpoint_site = TRUE },
  { NULL, NULL }
};

gchar *
debugger_builtin_fetch_command(void)
{
  gchar buf[1024];
  gsize len;

  printf("(syslog-ng) ");
  fflush(stdout);
  clearerr(stdin);

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
  gboolean requires_breakpoint_site = TRUE;
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
          requires_breakpoint_site = command_table[i].requires_breakpoint_site;
          break;
        }
    }
  if (!command)
    {
      printf("Undefined command %s, try \"help\"\n", argv[0]);
      return TRUE;
    }
  else if (requires_breakpoint_site && self->breakpoint_site == NULL)
    {
      printf("Running in interrupt context, command %s requires pipeline context\n", argv[0]);
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
  LogPipe *current_pipe;

  if (self->breakpoint_site)
    {
      current_pipe = self->breakpoint_site->pipe;

      printf("Breakpoint hit %s\n", log_expr_node_format_location(current_pipe->expr_node, buf, sizeof(buf)));
      _display_source_line(current_pipe->expr_node);
      _display_msg_with_template(self, self->breakpoint_site->msg, self->display_template);
    }
  else
    {
      printf("Stopping on interrupt, message related commands are unavailable...\n");
    }
  while (1)
    {
      _fetch_command(self);

      if (!_handle_command(self))
        break;

    }
  printf("(continuing)\n");
}

static gpointer
_debugger_thread_func(Debugger *self)
{
  app_thread_start();
  printf("Waiting for breakpoint...\n");
  while (1)
    {
      self->breakpoint_site = NULL;
      if (!tracer_wait_for_event(self->tracer, &self->breakpoint_site))
        break;

      _handle_interactive_prompt(self);
      tracer_resume_after_event(self->tracer, self->breakpoint_site);
    }
  scratch_buffers_explicit_gc();
  app_thread_stop();
  return NULL;
}

static void
_interrupt(gpointer user_data)
{
  Debugger *self = (Debugger *) user_data;

  tracer_stop_on_interrupt(self->tracer);
}

void
debugger_start_console(Debugger *self)
{
  main_loop_assert_main_thread();

  IV_SIGNAL_INIT(&self->sigint);
  self->sigint.signum = SIGINT;
  self->sigint.flags = IV_SIGNAL_FLAG_EXCLUSIVE;
  self->sigint.cookie = self;
  self->sigint.handler = _interrupt;
  iv_signal_register(&self->sigint);

  self->debugger_thread = g_thread_new(NULL, (GThreadFunc) _debugger_thread_func, self);
}

gboolean
debugger_stop_at_breakpoint(Debugger *self, LogPipe *pipe_, LogMessage *msg)
{
  BreakpointSite breakpoint_site = {0};
  msg_trace("Debugger: stopping at breakpoint",
            log_pipe_location_tag(pipe_));

  breakpoint_site.msg = log_msg_ref(msg);
  breakpoint_site.pipe = log_pipe_ref(pipe_);
  tracer_stop_on_breakpoint(self->tracer, &breakpoint_site);
  log_msg_unref(breakpoint_site.msg);
  log_pipe_unref(breakpoint_site.pipe);
  return !breakpoint_site.drop;
}

gboolean
debugger_perform_tracing(Debugger *self, LogPipe *pipe_, LogMessage *msg)
{
  struct timespec ts, *prev_ts = &self->last_trace_event;
  gchar buf[1024];

  clock_gettime(CLOCK_MONOTONIC, &ts);

  gint64 diff = prev_ts->tv_sec == 0 ? 0 : timespec_diff_nsec(&ts, prev_ts);
  printf("[%"G_GINT64_FORMAT".%09"G_GINT64_FORMAT" +%"G_GINT64_FORMAT"] Tracing %s\n",
         (gint64)ts.tv_sec, (gint64)ts.tv_nsec, diff,
         log_expr_node_format_location(pipe_->expr_node, buf, sizeof(buf)));
  *prev_ts = ts;
  return TRUE;
}

void
debugger_exit(Debugger *self)
{
  main_loop_assert_main_thread();

  iv_signal_unregister(&self->sigint);
  tracer_cancel(self->tracer);
  g_thread_join(self->debugger_thread);
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
