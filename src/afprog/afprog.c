/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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
  
#include "afprog.h"
#include "driver.h"
#include "messages.h"
#include "logwriter.h"
#include "children.h"
#include "misc.h"
#include "stats.h"

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

static gboolean
afprogram_popen(const gchar *cmdline, GIOCondition cond, pid_t *pid, gint *fd)
{
  int msg_pipe[2];
  
  g_return_val_if_fail(cond == G_IO_IN || cond == G_IO_OUT, FALSE);
  
  if (pipe(msg_pipe) == -1)
    {
      msg_error("Error creating program pipe",
                evt_tag_str("cmdline", cmdline),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return FALSE;
    }

  if ((*pid = fork()) < 0)
    {
      msg_error("Error in fork()",
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      close(msg_pipe[0]);
      close(msg_pipe[1]);
      return FALSE;
    }

  if (*pid == 0)
    {
      /* child */
      int devnull = open("/dev/null", O_WRONLY);
      
      if (devnull == -1)
        {
          _exit(127);
        }
        
      if (cond == G_IO_IN)
        {
          dup2(msg_pipe[1], 1);
          dup2(devnull, 0);
          dup2(devnull, 2);
        }
      else
        {
          dup2(msg_pipe[0], 0);
          dup2(devnull, 1);
          dup2(devnull, 2);
        }
      close(devnull);
      close(msg_pipe[0]);
      close(msg_pipe[1]);
      execl("/bin/sh", "/bin/sh", "-c", cmdline, NULL);
      _exit(127);
    }
  if (cond == G_IO_IN)
    {
      *fd = msg_pipe[0];
      close(msg_pipe[1]);
    }
  else
    {
      *fd = msg_pipe[1];
      close(msg_pipe[0]);
    }
  return TRUE;
}


/* source driver */

static void
afprogram_sd_exit(pid_t pid, int status, gpointer s)
{
  AFProgramSourceDriver *self = (AFProgramSourceDriver *) s;

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
    }
}

static gboolean
afprogram_sd_init(LogPipe *s)
{
  AFProgramSourceDriver *self = (AFProgramSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  gint fd;

  if (cfg)
    log_reader_options_init(&self->reader_options, cfg, self->super.group);
  
  msg_verbose("Starting source program",
              evt_tag_str("cmdline", self->cmdline->str),
              NULL); 
 
  if (!afprogram_popen(self->cmdline->str, G_IO_IN, &self->pid, &fd))
    return FALSE;

  /* parent */
  child_manager_register(self->pid, afprogram_sd_exit, log_pipe_ref(&self->super.super), (GDestroyNotify) log_pipe_unref);
  
  g_fd_set_nonblock(fd, TRUE);
  if (!self->reader)
    {
      LogTransport *transport;

      transport = log_transport_plain_new(fd, 0);
      transport->timeout = 10;
      self->reader = log_reader_new(log_proto_text_server_new(transport, self->reader_options.msg_size, 0));
      log_reader_set_options(self->reader, s, &self->reader_options, 0, SCS_PROGRAM, self->super.id, self->cmdline->str);
    }
  log_pipe_append(self->reader, &self->super.super);
  if (!log_pipe_init(self->reader, NULL))
    { 
      msg_error("Error initializing program source, closing fd",
                evt_tag_int("fd", fd),
                NULL);
      log_pipe_unref(self->reader);
      self->reader = NULL;
      close(fd);
      return FALSE;
    }
  return TRUE;
}

static gboolean
afprogram_sd_deinit(LogPipe *s)
{
  AFProgramSourceDriver *self = (AFProgramSourceDriver *) s;

  if (self->pid != -1)
    {
      msg_verbose("Sending source program a TERM signal",
                  evt_tag_str("cmdline", self->cmdline->str),
                  evt_tag_int("child_pid", self->pid),
                  NULL);
      kill(self->pid, SIGTERM);
      self->pid = -1;
    }

  if (self->reader)
    {
      log_pipe_deinit(self->reader);
      log_pipe_unref(self->reader);
      self->reader = NULL;
    }
  return TRUE;
}

static void
afprogram_sd_free(LogPipe *s)
{
  AFProgramSourceDriver *self = (AFProgramSourceDriver *) s;
  
  log_reader_options_destroy(&self->reader_options);
  g_string_free(self->cmdline, TRUE);
  log_drv_free(s);
}

static void
afprogram_sd_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  switch (notify_code)
    {
      case NC_CLOSE:
      case NC_READ_ERROR:
        afprogram_sd_deinit(s);
        afprogram_sd_init(s);
        break;
    }
}

LogDriver *
afprogram_sd_new(gchar *cmdline)
{
  AFProgramSourceDriver *self = g_new0(AFProgramSourceDriver, 1);
  log_drv_init_instance(&self->super);
  
  self->super.super.init = afprogram_sd_init;
  self->super.super.deinit = afprogram_sd_deinit;
  self->super.super.free_fn = afprogram_sd_free;
  self->super.super.notify = afprogram_sd_notify;
  self->cmdline = g_string_new(cmdline);
  log_reader_options_defaults(&self->reader_options);
  self->reader_options.flags |= LR_LOCAL;
  return &self->super;
}

/* dest driver */

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
      log_pipe_deinit(&self->super.super);
      log_pipe_init(&self->super.super, NULL);
    }
}

static gboolean
afprogram_dd_init(LogPipe *s)
{
  AFProgramDestDriver *self = (AFProgramDestDriver *) s;
  int fd;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (cfg)
    log_writer_options_init(&self->writer_options, cfg, 0);
  
  msg_verbose("Starting destination program",
              evt_tag_str("cmdline", self->cmdline->str),
              NULL);
              
  if (!afprogram_popen(self->cmdline->str, G_IO_OUT, &self->pid, &fd))
    return FALSE;

  /* parent */
  
  child_manager_register(self->pid, afprogram_dd_exit, log_pipe_ref(&self->super.super), (GDestroyNotify) log_pipe_unref);
  
  g_fd_set_nonblock(fd, TRUE);
  if (!self->writer)
    {
      self->writer = log_writer_new(LW_FORMAT_FILE);
      log_writer_set_options((LogWriter *) self->writer, s, &self->writer_options, 0, SCS_PROGRAM, self->super.id, self->cmdline->str);
    }
  log_writer_reopen(self->writer, log_proto_text_client_new(log_transport_plain_new(fd, 0)));
  log_pipe_init(self->writer, NULL);
  log_pipe_append(&self->super.super, self->writer);
  return TRUE;
}

static gboolean
afprogram_dd_deinit(LogPipe *s)
{
  AFProgramDestDriver *self = (AFProgramDestDriver *) s;

  if (self->pid != -1)
    {
      msg_verbose("Sending destination program a TERM signal",
                  evt_tag_str("cmdline", self->cmdline->str),
                  evt_tag_int("child_pid", self->pid),
                  NULL);
      kill(self->pid, SIGTERM);
      self->pid = -1;
    }
  if (self->writer)
    log_pipe_deinit(self->writer);
  return TRUE;
}

static void
afprogram_dd_free(LogPipe *s)
{
  AFProgramDestDriver *self = (AFProgramDestDriver *) s;

  log_pipe_unref(self->writer);  
  g_string_free(self->cmdline, TRUE);
  log_writer_options_destroy(&self->writer_options);
  log_drv_free(s);
}

static void
afprogram_dd_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  switch (notify_code)
    {
    case NC_CLOSE:
    case NC_WRITE_ERROR:
      afprogram_dd_deinit(s);
      afprogram_dd_init(s);
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
