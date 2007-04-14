/*
 * Copyright (c) 2002, 2003, 2004 BalaBit IT Ltd, Budapest, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "afprog.h"
#include "driver.h"
#include "messages.h"
#include "logwriter.h"
#include "children.h"
#include "misc.h"

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

static const gchar *
afprogram_dd_format_stats_name(AFProgramDestDriver *self)
{
  static gchar stats_name[256];
  
  g_snprintf(stats_name, sizeof(stats_name),
             "program(%s)", self->cmdline->str);
  
  return stats_name;
}

static void
afprogram_dd_exit(pid_t pid, int status, gpointer s)
{
  AFProgramDestDriver *self = (AFProgramDestDriver *) s;

  /* Note: self->pid being -1 means that deinit was called, thus we don't
   * need to restart the command. self->pid might change due to EPIPE
   * handling restarting the command before this handler is run. */
  if (self->pid != -1 && self->pid == pid)
    {
      msg_verbose("Child program exited, restarting",
                  evt_tag_str("cmdline", self->cmdline->str),
                  evt_tag_int("status", status),
                  NULL);
      self->pid = -1;
      log_pipe_deinit(&self->super.super, NULL, NULL);
      log_pipe_init(&self->super.super, NULL, NULL);
    }
}

static gboolean
afprogram_dd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFProgramDestDriver *self = (AFProgramDestDriver *) s;
  int msg_pipe[2];

  if (cfg)
    log_writer_options_init(&self->writer_options, cfg, 0, afprogram_dd_format_stats_name(self));
  
  msg_verbose("Starting destination program",
              evt_tag_str("cmdline", self->cmdline->str),
              NULL);  
  if (pipe(msg_pipe) == -1)
    {
      msg_error("Error creating program pipe",
                evt_tag_str("cmdline", self->cmdline->str),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return FALSE;
    }
  if ((self->pid = fork()) < 0)
    {
      msg_error("Error in fork()",
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      close(msg_pipe[0]);
      close(msg_pipe[1]);
      return FALSE;
    }
  if (self->pid == 0)
    {
      /* child */
      int devnull = open("/dev/null", O_WRONLY);
      
      if (devnull == -1)
        {
          _exit(127);
        }
      dup2(msg_pipe[0], 0);
      dup2(devnull, 1);
      dup2(devnull, 2);
      close(devnull);
      close(msg_pipe[0]);
      close(msg_pipe[1]);
      execl("/bin/sh", "/bin/sh", "-c", self->cmdline->str, NULL);
      _exit(127);
    }
  else
    {
      /* parent */
      
      child_manager_register(self->pid, afprogram_dd_exit, log_pipe_ref(&self->super.super), (GDestroyNotify) log_pipe_unref);
      
      close(msg_pipe[0]);
      if (!self->writer)
        self->writer = log_writer_new(LW_FORMAT_FILE, s, &self->writer_options);
      log_writer_reopen(self->writer, fd_write_new(msg_pipe[1]));
      log_pipe_init(self->writer, NULL, NULL);
      log_pipe_append(&self->super.super, self->writer);
    }
  return TRUE;
}

static gboolean
afprogram_dd_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFProgramDestDriver *self = (AFProgramDestDriver *) s;

  if (self->pid != -1)
    {
      msg_verbose("Sending child a TERM signal",
                  evt_tag_int("child_pid", self->pid),
                  NULL);
      kill(self->pid, SIGTERM);
      self->pid = -1;
    }
  if (self->writer)
    log_pipe_deinit(self->writer, NULL, NULL);
  return TRUE;
}

static void
afprogram_dd_free(LogPipe *s)
{
  AFProgramDestDriver *self = (AFProgramDestDriver *) s;

  log_pipe_unref(self->writer);  
  g_string_free(self->cmdline, TRUE);
  log_writer_options_destroy(&self->writer_options);
  log_drv_free_instance(&self->super);
  g_free(self);
}

static void
afprogram_dd_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  switch (notify_code)
    {
    case NC_CLOSE:
    case NC_WRITE_ERROR:
      afprogram_dd_deinit(s, NULL, NULL);
      afprogram_dd_init(s, NULL, NULL);
      break;
    }
}

LogDriver *
afprogram_dd_new(gchar *cmdline)
{
  AFProgramDestDriver *self = g_new0(AFProgramDestDriver, 1);
  log_drv_init_instance(&self->super);
  
  self->super.super.init = afprogram_dd_init;
  self->super.super.deinit = afprogram_dd_deinit;
  self->super.super.free_fn = afprogram_dd_free;
  self->super.super.notify = afprogram_dd_notify;
  self->cmdline = g_string_new(cmdline);
  log_writer_options_defaults(&self->writer_options);
  return &self->super;
}
