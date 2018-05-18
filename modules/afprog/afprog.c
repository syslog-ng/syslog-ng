/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
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
#include "fdhelpers.h"
#include "stats/stats-registry.h"
#include "transport/transport-pipe.h"
#include "logproto/logproto-text-server.h"
#include "logproto/logproto-text-client.h"
#include "poll-fd-events.h"

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

typedef struct _AFProgramReloadStoreItem
{
  LogWriter *writer;
  pid_t pid;
} AFProgramReloadStoreItem;

static inline void
_terminate_process_group_by_pid(const pid_t pid)
{
  msg_verbose("Sending TERM signal to the process group",
              evt_tag_int("pid", pid));

  pid_t pgid = getpgid(pid);
  if (pgid != -1 && pgid != getpgrp())
    killpg(pgid, SIGTERM);
}

static inline void
afprogram_reload_store_item_deinit(AFProgramReloadStoreItem *reload_info)
{
  child_manager_unregister(reload_info->pid);
  _terminate_process_group_by_pid(reload_info->pid);
}

static inline void
afprogram_reload_store_item_free(AFProgramReloadStoreItem *reload_info)
{
  log_pipe_unref((LogPipe *)reload_info->writer);
  g_free(reload_info);
}

static inline void
afprogram_reload_store_item_destroy_notify(gpointer data)
{
  AFProgramReloadStoreItem *reload_info = (AFProgramReloadStoreItem *)data;

  afprogram_reload_store_item_deinit(reload_info);
  afprogram_reload_store_item_free(reload_info);
}

static inline void
_exec_program_with_clean_environment(const gchar *cmdline)
{
  execle("/bin/sh", "/bin/sh", "-c", cmdline, NULL, NULL);
}

static inline void
_exec_program(const gchar *cmdline)
{
  execl("/bin/sh", "/bin/sh", "-c", cmdline, NULL);
}

static gboolean
afprogram_popen(AFProgramProcessInfo *process_info, GIOCondition cond, gint *fd)
{
  int msg_pipe[2];

  g_return_val_if_fail(cond == G_IO_IN || cond == G_IO_OUT, FALSE);

  if (pipe(msg_pipe) == -1)
    {
      msg_error("Error creating program pipe",
                evt_tag_str("cmdline", process_info->cmdline->str),
                evt_tag_error(EVT_TAG_OSERROR));
      return FALSE;
    }

  if ((process_info->pid = fork()) < 0)
    {
      msg_error("Error in fork()",
                evt_tag_error(EVT_TAG_OSERROR));
      close(msg_pipe[0]);
      close(msg_pipe[1]);
      return FALSE;
    }

  if (process_info->pid == 0)
    {
      /* child */
      int devnull;

      setpgid(0, 0);

      devnull = open("/dev/null", O_WRONLY);

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

      if (process_info->inherit_environment)
        _exec_program(process_info->cmdline->str);
      else
        _exec_program_with_clean_environment(process_info->cmdline->str);

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
  msg_verbose(cond == G_IO_IN ? "Program source started" : "Program destination started",
              evt_tag_str("cmdline", process_info->cmdline->str),
              evt_tag_int("fd", *fd));
  return TRUE;
}


/* source driver */

static void
afprogram_sd_kill_child(AFProgramSourceDriver *self)
{
  if (self->process_info.pid != -1)
    {
      msg_verbose("Sending source program a TERM signal",
                  evt_tag_str("cmdline", self->process_info.cmdline->str),
                  evt_tag_int("child_pid", self->process_info.pid));
      _terminate_process_group_by_pid(self->process_info.pid);
      self->process_info.pid = -1;
    }
}

static void
afprogram_sd_exit(pid_t pid, int status, gpointer s)
{
  AFProgramSourceDriver *self = (AFProgramSourceDriver *) s;

  /* Note: self->process_info.pid being -1 means that deinit was called, thus we don't
   * need to restart the command. self->process_info.pid might change due to EPIPE
   * handling restarting the command before this handler is run. */
  if (self->process_info.pid != -1 && self->process_info.pid == pid)
    {
      msg_verbose("Child program exited",
                  evt_tag_str("cmdline", self->process_info.cmdline->str),
                  evt_tag_int("status", status));
      self->process_info.pid = -1;
    }
}

static gboolean
afprogram_sd_init(LogPipe *s)
{
  AFProgramSourceDriver *self = (AFProgramSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  gint fd;

  if (!log_src_driver_init_method(s))
    return FALSE;

  if (cfg)
    log_reader_options_init(&self->reader_options, cfg, self->super.super.group);

  msg_verbose("Starting source program",
              evt_tag_str("cmdline", self->process_info.cmdline->str));

  if (!afprogram_popen(&self->process_info, G_IO_IN, &fd))
    return FALSE;

  /* parent */
  child_manager_register(self->process_info.pid, afprogram_sd_exit, log_pipe_ref(&self->super.super.super),
                         (GDestroyNotify) log_pipe_unref);

  g_fd_set_nonblock(fd, TRUE);
  g_fd_set_cloexec(fd, TRUE);
  if (!self->reader)
    {
      LogTransport *transport;
      LogProtoServer *proto;

      transport = log_transport_pipe_new(fd);
      proto = log_proto_text_server_new(transport, &self->reader_options.proto_options.super);

      self->reader = log_reader_new(s->cfg);
      log_reader_reopen(self->reader, proto,
                        poll_fd_events_new(fd));
      log_reader_set_options(self->reader,
                             s,
                             &self->reader_options,
                             self->super.super.id,
                             self->process_info.cmdline->str);
    }
  log_pipe_append((LogPipe *) self->reader, &self->super.super.super);
  if (!log_pipe_init((LogPipe *) self->reader))
    {
      msg_error("Error initializing program source, closing fd",
                evt_tag_int("fd", fd));
      log_pipe_unref((LogPipe *) self->reader);
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

  afprogram_sd_kill_child(self);
  if (self->reader)
    {
      log_pipe_deinit((LogPipe *) self->reader);
      log_pipe_unref((LogPipe *) self->reader);
      self->reader = NULL;
    }

  if (!log_src_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

static void
afprogram_sd_free(LogPipe *s)
{
  AFProgramSourceDriver *self = (AFProgramSourceDriver *) s;

  log_reader_options_destroy(&self->reader_options);
  g_string_free(self->process_info.cmdline, TRUE);
  log_src_driver_free(s);
}

static void
afprogram_sd_notify(LogPipe *s, gint notify_code, gpointer user_data)
{
  switch (notify_code)
    {
    case NC_CLOSE:
    case NC_READ_ERROR:
      afprogram_sd_deinit(s);
      afprogram_sd_init(s);
      break;
    default:
      break;
    }
}

LogDriver *
afprogram_sd_new(gchar *cmdline, GlobalConfig *cfg)
{
  AFProgramSourceDriver *self = g_new0(AFProgramSourceDriver, 1);
  log_src_driver_init_instance(&self->super, cfg);

  self->super.super.super.init = afprogram_sd_init;
  self->super.super.super.deinit = afprogram_sd_deinit;
  self->super.super.super.free_fn = afprogram_sd_free;
  self->super.super.super.notify = afprogram_sd_notify;
  self->process_info.cmdline = g_string_new(cmdline);
  afprogram_set_inherit_environment(&self->process_info, TRUE);
  log_reader_options_defaults(&self->reader_options);
  self->reader_options.parse_options.flags |= LP_LOCAL;
  self->reader_options.super.stats_level = STATS_LEVEL0;
  self->reader_options.super.stats_source = SCS_PROGRAM;
  return &self->super.super;
}

/* dest driver */

static void afprogram_dd_exit(pid_t pid, int status, gpointer s);

static gchar *
afprogram_dd_format_queue_persist_name(AFProgramDestDriver *self)
{
  static gchar persist_name[256];

  g_snprintf(persist_name, sizeof(persist_name),
             "afprogram_dd_qname(%s,%s)", self->process_info.cmdline->str, self->super.super.id);

  return persist_name;
}

static const gchar *
afprogram_dd_format_persist_name(const LogPipe *s)
{
  const AFProgramDestDriver *self = (const AFProgramDestDriver *)s;
  static gchar persist_name[256];

  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "afprogram_dd_name.%s", s->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "afprogram_dd_name(%s,%s)",
               self->process_info.cmdline->str, self->super.super.id);

  return persist_name;
}

static void
afprogram_dd_kill_child(AFProgramDestDriver *self)
{
  if (self->process_info.pid != -1)
    {
      msg_verbose("Sending destination program a TERM signal",
                  evt_tag_str("cmdline", self->process_info.cmdline->str),
                  evt_tag_int("child_pid", self->process_info.pid));
      _terminate_process_group_by_pid(self->process_info.pid);
      self->process_info.pid = -1;
    }
}

static inline gboolean
afprogram_dd_open_program(AFProgramDestDriver *self, int *fd)
{
  if (self->process_info.pid == -1)
    {
      msg_verbose("Starting destination program",
                  evt_tag_str("cmdline", self->process_info.cmdline->str));

      if (!afprogram_popen(&self->process_info, G_IO_OUT, fd))
        return FALSE;

      g_fd_set_nonblock(*fd, TRUE);
    }

  child_manager_register(self->process_info.pid, afprogram_dd_exit, log_pipe_ref(&self->super.super.super),
                         (GDestroyNotify)log_pipe_unref);

  return TRUE;
}

static gboolean
afprogram_dd_reopen(AFProgramDestDriver *self)
{
  int fd;

  afprogram_dd_kill_child(self);

  if (!afprogram_dd_open_program(self, &fd))
    return FALSE;

  log_writer_reopen(self->writer, log_proto_text_client_new(log_transport_pipe_new(fd),
                                                            &self->writer_options.proto_options.super));
  return TRUE;
}

static gboolean
afprogram_dd_is_command_not_found(int status)
{
  /*
   * Status here is a 16-bit int, the real exit status is the 8 least
   * significant bits of it, and an exit status of 127 from the shell is
   * supposed to signal a command-not-found case. The other 8 bits are one bit
   * to signal if the child dumped core, the other 7 the signal - if any - with
   * which it exited.
   *
   * In our case, we want to make sure that the other 8 bits are all empty (no
   * core dump; and normal exit, not one due to a signal), hence the second part
   * of the check.
   *
   * See wait(2) for more information.
   */
  return (status >> 8) == 127 && ((status & 0x00ff) == 0);
}

static void
afprogram_dd_exit(pid_t pid, int status, gpointer s)
{
  AFProgramDestDriver *self = (AFProgramDestDriver *) s;

  /* Note: self->process_info.pid being -1 means that deinit was called, thus we don't
   * need to restart the command. self->process_info.pid might change due to EPIPE
   * handling restarting the command before this handler is run. */
  if (self->process_info.pid != -1 && self->process_info.pid == pid)
    {
      if (afprogram_dd_is_command_not_found(status))
        {
          msg_error("Child program exited with command not found, stopping the destination.",
                    evt_tag_str("cmdline", self->process_info.cmdline->str),
                    evt_tag_int("status", status));

          self->process_info.pid = -1;
        }
      else
        {
          msg_info("Child program exited, restarting",
                   evt_tag_str("cmdline", self->process_info.cmdline->str),
                   evt_tag_int("status", status));

          self->process_info.pid = -1;
          afprogram_dd_reopen(self);
        }
    }
}

static gboolean
afprogram_dd_restore_reload_store_item(AFProgramDestDriver *self, GlobalConfig *cfg)
{
  const gchar *persist_name = afprogram_dd_format_persist_name((const LogPipe *)self);
  AFProgramReloadStoreItem *restored_info =
    (AFProgramReloadStoreItem *)cfg_persist_config_fetch(cfg, persist_name);

  if (restored_info)
    {
      self->process_info.pid = restored_info->pid;
      self->writer = restored_info->writer;

      child_manager_register(self->process_info.pid, afprogram_dd_exit, log_pipe_ref(&self->super.super.super),
                             (GDestroyNotify)log_pipe_unref);
      g_free(restored_info);
    }

  return !!(self->writer);
}

static gboolean
afprogram_dd_init(LogPipe *s)
{
  AFProgramDestDriver *self = (AFProgramDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_dest_driver_init_method(s))
    return FALSE;

  log_writer_options_init(&self->writer_options, cfg, 0);

  const gboolean restore_successful = afprogram_dd_restore_reload_store_item(self, cfg);

  if (!self->writer)
    self->writer = log_writer_new(LW_FORMAT_FILE, s->cfg);

  log_writer_set_options(self->writer,
                         s,
                         &self->writer_options,
                         self->super.super.id,
                         self->process_info.cmdline->str);
  log_writer_set_queue(self->writer, log_dest_driver_acquire_queue(&self->super,
                       afprogram_dd_format_queue_persist_name(self)));

  if (!log_pipe_init((LogPipe *) self->writer))
    {
      log_pipe_unref((LogPipe *) self->writer);
      return FALSE;
    }
  log_pipe_append(&self->super.super.super, (LogPipe *) self->writer);

  return restore_successful ? TRUE : afprogram_dd_reopen(self);
}

static inline void
afprogram_dd_store_reload_store_item(AFProgramDestDriver *self, GlobalConfig *cfg)
{
  AFProgramReloadStoreItem *reload_info = g_new0(AFProgramReloadStoreItem, 1);

  reload_info->pid = self->process_info.pid;
  reload_info->writer = self->writer;

  cfg_persist_config_add(cfg, afprogram_dd_format_persist_name((const LogPipe *)self), reload_info,
                         afprogram_reload_store_item_destroy_notify, FALSE);
}

static gboolean
afprogram_dd_deinit(LogPipe *s)
{
  AFProgramDestDriver *self = (AFProgramDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  if (self->writer)
    log_pipe_deinit((LogPipe *) self->writer);

  child_manager_unregister(self->process_info.pid);

  if (self->keep_alive)
    {
      afprogram_dd_store_reload_store_item(self, cfg);
    }
  else
    {
      afprogram_dd_kill_child(self);

      if (self->writer)
        log_pipe_unref((LogPipe *) self->writer);
    }

  if (self->writer)
    {
      self->writer = NULL;
    }

  return log_dest_driver_deinit_method(s);
}

static void
afprogram_dd_free(LogPipe *s)
{
  AFProgramDestDriver *self = (AFProgramDestDriver *) s;

  log_pipe_unref((LogPipe *) self->writer);
  g_string_free(self->process_info.cmdline, TRUE);
  log_writer_options_destroy(&self->writer_options);
  log_dest_driver_free(s);
}

static void
afprogram_dd_notify(LogPipe *s, gint notify_code, gpointer user_data)
{
  AFProgramDestDriver *self = (AFProgramDestDriver *) s;

  switch (notify_code)
    {
    case NC_CLOSE:
      afprogram_dd_reopen(self);
      break;
    case NC_WRITE_ERROR:
      /* We let this fall through, to be handled by the child manager. We do
         this to have access to the exit status, and which we use to decide
         whether to restart immediately, or stop the destination. */
      break;
    default:
      break;
    }
}

LogDriver *
afprogram_dd_new(gchar *cmdline, GlobalConfig *cfg)
{
  AFProgramDestDriver *self = g_new0(AFProgramDestDriver, 1);
  log_dest_driver_init_instance(&self->super, cfg);

  self->super.super.super.init = afprogram_dd_init;
  self->super.super.super.deinit = afprogram_dd_deinit;
  self->super.super.super.free_fn = afprogram_dd_free;
  self->super.super.super.notify = afprogram_dd_notify;
  self->super.super.super.generate_persist_name = afprogram_dd_format_persist_name;
  self->process_info.cmdline = g_string_new(cmdline);
  self->process_info.pid = -1;
  afprogram_set_inherit_environment(&self->process_info, TRUE);
  log_writer_options_defaults(&self->writer_options);
  self->writer_options.stats_level = STATS_LEVEL0;
  self->writer_options.stats_source = SCS_PROGRAM;
  return &self->super.super;
}

void
afprogram_dd_set_keep_alive(AFProgramDestDriver *self, gboolean keep_alive)
{
  self->keep_alive = keep_alive;
}

void
afprogram_set_inherit_environment(AFProgramProcessInfo *self, gboolean inherit_environment)
{
  self->inherit_environment = inherit_environment;
}
