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
#include "affile.h"
#include "driver.h"
#include "messages.h"
#include "misc.h"
#include "serialize.h"
#include "gprocess.h"
#include "stats.h"
#include "mainloop.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

static gboolean
affile_open_file(gchar *name, gint flags,
                 gint uid, gint gid, gint mode,
                 gint dir_uid, gint dir_gid, gint dir_mode,
                 gboolean create_dirs, gboolean privileged, gboolean is_pipe, gint *fd)
{
  cap_t saved_caps;
  struct stat st;

  if (strstr(name, "../") || strstr(name, "/..")) 
    {
      msg_error("Spurious path, logfile not created",
                evt_tag_str("path", name),
                NULL);
      return FALSE;
    }

  saved_caps = g_process_cap_save();
  if (privileged)
    {
      g_process_cap_modify(CAP_DAC_READ_SEARCH, TRUE);
      g_process_cap_modify(CAP_SYSLOG, TRUE);
    }
  else
    {
      g_process_cap_modify(CAP_DAC_OVERRIDE, TRUE);
    }

  if (create_dirs && !create_containing_directory(name, dir_uid, dir_gid, dir_mode))
    {
      g_process_cap_restore(saved_caps);
      return FALSE;
    }

  *fd = -1;
  if (stat(name, &st) >= 0)
    {
      if (is_pipe && !S_ISFIFO(st.st_mode))
        {
          msg_warning("WARNING: you are using the pipe driver, underlying file is not a FIFO, it should be used by file()",
                    evt_tag_str("filename", name),
                    NULL);
        }
      else if (!is_pipe && S_ISFIFO(st.st_mode))
        {
          msg_warning("WARNING: you are using the file driver, underlying file is a FIFO, it should be used by pipe()",
                      evt_tag_str("filename", name),
                      NULL);
        }
    }
  *fd = open(name, flags, mode < 0 ? 0600 : mode);
  if (is_pipe && *fd < 0 && errno == ENOENT)
    {
      if (mkfifo(name, 0666) >= 0)
        *fd = open(name, flags, 0666);
    }

  if (*fd != -1)
    {
      g_fd_set_cloexec(*fd, TRUE);
      
      g_process_cap_modify(CAP_CHOWN, TRUE);
      g_process_cap_modify(CAP_FOWNER, TRUE);
      set_permissions_fd(*fd, uid, gid, mode);
    }
  g_process_cap_restore(saved_caps);
  msg_trace("affile_open_file",
            evt_tag_str("path", name),
            evt_tag_int("fd",*fd),
            NULL);

  return *fd != -1;
}

static gboolean
affile_sd_open_file(AFFileSourceDriver *self, gchar *name, gint *fd)
{
  gint flags;
  
  if (self->flags & AFFILE_PIPE)
    flags = O_RDWR | O_NOCTTY | O_NONBLOCK | O_LARGEFILE;
  else
    flags = O_RDONLY | O_NOCTTY | O_NONBLOCK | O_LARGEFILE;

  if (affile_open_file(name, flags, -1, -1, -1, 0, 0, 0, 0, !!(self->flags & AFFILE_PRIVILEGED), !!(self->flags & AFFILE_PIPE), fd))
    return TRUE;
  return FALSE;
}

static inline gchar *
affile_sd_format_persist_name(AFFileSourceDriver *self)
{
  static gchar persist_name[1024];
  
  g_snprintf(persist_name, sizeof(persist_name), "affile_sd_curpos(%s)", self->filename->str);
  return persist_name;
}
 
static void
affile_sd_recover_state(LogPipe *s, GlobalConfig *cfg, LogProto *proto)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  if ((self->flags & AFFILE_PIPE) || self->reader_options.follow_freq <= 0)
    return;

  if (!log_proto_restart_with_state(proto, cfg->state, affile_sd_format_persist_name(self)))
    {
      msg_error("Error converting persistent state from on-disk format, losing file position information",
                evt_tag_str("filename", self->filename->str),
                NULL);
      return;
    }
}

static LogProto *
affile_sd_construct_proto(AFFileSourceDriver *self, LogTransport *transport)
{
  guint flags;
  LogProto *proto;
  MsgFormatHandler *handler;

  flags =
    ((self->reader_options.follow_freq > 0)
     ? LPBS_IGNORE_EOF | LPBS_POS_TRACKING
     : LPBS_NOMREAD);

  handler = self->reader_options.parse_options.format_handler;
  if ((handler && handler->construct_proto))
    proto = self->reader_options.parse_options.format_handler->construct_proto(&self->reader_options.parse_options, transport, flags);
  else if (self->reader_options.padding)
    proto = log_proto_record_server_new(transport, self->reader_options.padding, flags);
  else
    proto = log_proto_text_server_new(transport, self->reader_options.msg_size, flags);

  return proto;
}

/* NOTE: runs in the main thread */
static void
affile_sd_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  gint fd;
  
  switch (notify_code)
    {
    case NC_FILE_MOVED:
      { 
        msg_verbose("Follow-mode file source moved, tracking of the new file is started",
                    evt_tag_str("filename", self->filename->str),
                    NULL);
        
        log_pipe_deinit(self->reader);
        log_pipe_unref(self->reader);
        
        if (affile_sd_open_file(self, self->filename->str, &fd))
          {
            LogTransport *transport;
            LogProto *proto;
            
            transport = log_transport_plain_new(fd, 0);
            transport->timeout = 10;

            proto = affile_sd_construct_proto(self, transport);

            self->reader = log_reader_new(proto);

            log_reader_set_options(self->reader, s, &self->reader_options, 1, SCS_FILE, self->super.super.id, self->filename->str);
            log_reader_set_follow_filename(self->reader, self->filename->str);
            log_reader_set_immediate_check(self->reader);

            log_pipe_append(self->reader, s);
            if (!log_pipe_init(self->reader, cfg))
              {
                msg_error("Error initializing log_reader, closing fd",
                          evt_tag_int("fd", fd),
                          NULL);
                log_pipe_unref(self->reader);
                self->reader = NULL;
                close(fd);
              }
            affile_sd_recover_state(s, cfg, proto);
          }
        else
          {
            self->reader = NULL;
          }
        break;
      }
    default:
      break;
    }
}

static void
affile_sd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  static NVHandle filename_handle = 0;

  if (!filename_handle)
    filename_handle = log_msg_get_value_handle("FILE_NAME");
  
  log_msg_set_value(msg, filename_handle, self->filename->str, self->filename->len);

  log_pipe_forward_msg(s, msg, path_options);
}

static gboolean
affile_sd_init(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  gint fd;
  gboolean file_opened, open_deferred = FALSE;

  if (!log_src_driver_init_method(s))
    return FALSE;

  log_reader_options_init(&self->reader_options, cfg, self->super.super.group);

  file_opened = affile_sd_open_file(self, self->filename->str, &fd);
  if (!file_opened && self->reader_options.follow_freq > 0)
    {
      msg_info("Follow-mode file source not found, deferring open",
               evt_tag_str("filename", self->filename->str),
               NULL);
      open_deferred = TRUE;
      fd = -1;
    }

  if (file_opened || open_deferred)
    {
      LogTransport *transport;
      LogProto *proto;

      transport = log_transport_plain_new(fd, 0);
      transport->timeout = 10;

      proto = affile_sd_construct_proto(self, transport);
      /* FIXME: we shouldn't use reader_options to store log protocol parameters */
      self->reader = log_reader_new(proto);

      log_reader_set_options(self->reader, s, &self->reader_options, 1, SCS_FILE, self->super.super.id, self->filename->str);
      log_reader_set_follow_filename(self->reader, self->filename->str);

      /* NOTE: if the file could not be opened, we ignore the last
       * remembered file position, if the file is created in the future
       * we're going to read from the start. */
      
      log_pipe_append(self->reader, s);

      if (!log_pipe_init(self->reader, NULL))
        {
          msg_error("Error initializing log_reader, closing fd",
                    evt_tag_int("fd", fd),
                    NULL);
          log_pipe_unref(self->reader);
          self->reader = NULL;
          close(fd);
          return FALSE;
        }
      affile_sd_recover_state(s, cfg, proto);
    }
  else
    {
      msg_error("Error opening file for reading",
                evt_tag_str("filename", self->filename->str),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return self->super.super.optional;
    }
  return TRUE;

}

static gboolean
affile_sd_deinit(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  if (self->reader)
    {
      log_pipe_deinit(self->reader);
      log_pipe_unref(self->reader);
      self->reader = NULL;
    }

  if (!log_src_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

static void
affile_sd_free(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  g_string_free(self->filename, TRUE);
  g_assert(!self->reader);

  log_reader_options_destroy(&self->reader_options);

  log_src_driver_free(s);
}

LogDriver *
affile_sd_new(gchar *filename, guint32 flags)
{
  AFFileSourceDriver *self = g_new0(AFFileSourceDriver, 1);
  
  log_src_driver_init_instance(&self->super);
  self->filename = g_string_new(filename);
  self->flags = flags;
  self->super.super.super.init = affile_sd_init;
  self->super.super.super.queue = affile_sd_queue;
  self->super.super.super.deinit = affile_sd_deinit;
  self->super.super.super.notify = affile_sd_notify;
  self->super.super.super.free_fn = affile_sd_free;
  log_reader_options_defaults(&self->reader_options);
  self->reader_options.flags |= LR_LOCAL;

  if ((self->flags & AFFILE_PIPE))
    {
      static gboolean warned = FALSE;

      if (configuration && configuration->version < 0x0302)
        {
          if (!warned)
            {
              msg_warning("WARNING: the expected message format is being changed for pipe() to improve "
                          "syslogd compatibity with syslog-ng 3.2. If you are using custom "
                          "applications which bypass the syslog() API, you might "
                          "need the 'expect-hostname' flag to get the old behaviour back", NULL);
              warned = TRUE;
            }
        }
      else
        {
          self->reader_options.parse_options.flags &= ~LP_EXPECT_HOSTNAME;
        }
    }
  
  if (configuration && configuration->version < 0x0300)
    {
      static gboolean warned = FALSE;
      
      if (!warned)
        {
          msg_warning("WARNING: file source: default value of follow_freq in file sources is changing in 3.0 to '1' for all files except /proc/kmsg",
                      NULL);
          warned = TRUE;
        }
    }
  else
    {
      if ((self->flags & AFFILE_PIPE) == 0)
        {
          if (0 ||
#if __linux__
              (strcmp(filename, "/proc/kmsg") == 0) ||
#elif __FreeBSD__
              (strcmp(filename, "/dev/klog") == 0) ||
#endif
               0)
            {
              self->reader_options.follow_freq = 0;
            }
          else
            {
              self->reader_options.follow_freq = 1000;
            }
        }
    }
#if __linux__
  if (strcmp(filename, "/proc/kmsg") == 0)
    {
      self->flags |= AFFILE_PRIVILEGED;
    }
#endif
  return &self->super.super;
}

/*
 * Threading notes:
 *
 * Apart from standard initialization/deinitialization (normally performed
 * by the main thread when syslog-ng starts up) the following processes are
 * performed in various threads.
 *
 *   - queue runs in the thread of the source thread that generated the message
 *   - if the message is to be written to a not-yet-opened file, a new gets
 *     opened and stored in the writer_hash hashtable (initiated from queue,
 *     but performed in the main thread, but more on that later)
 *   - currently opened destination files are checked regularly and closed
 *     if they are idle for a given amount of time (time_reap) (this is done
 *     in the main thread)
 *
 * Some of these operations have to be performed in the main thread, others
 * are done in the queue call.
 *
 * References
 * ==========
 *
 * The AFFileDestDriver instance is registered into the current
 * configuration, thus its presence is always given, it cannot go away while
 * syslog-ng is running.
 *
 * AFFileDestWriter instances are created dynamically when a new file is
 * opened. A reference is stored in the writer_hash hashtable. This is then:
 *    - looked up in _queue() (in the source thread)
 *    - cleaned up in reap callback (in the main thread)
 *
 * writer_hash is locked (currently a simple mutex) using
 * AFFileDestDriver->lock.  The "queue" method cannot hold the lock while
 * forwarding it to the next pipe, thus a reference is taken under the
 * protection of the lock, keeping a the next pipe alive, even if that would
 * go away in a parallel reaper process.
 */

struct _AFFileDestWriter
{
  LogPipe super;
  GStaticMutex lock;
  AFFileDestDriver *owner;
  gchar *filename;
  LogPipe *writer;
  time_t last_msg_stamp;
  time_t last_open_stamp;
  time_t time_reopen;
  struct iv_timer reap_timer;
  gboolean reopen_pending, queue_pending;
};

static void affile_dd_reap_writer(AFFileDestDriver *self, AFFileDestWriter *dw);

static void
affile_dw_arm_reaper(AFFileDestWriter *self)
{
  /* not yet reaped, set up the next callback */
  iv_validate_now();
  self->reap_timer.expires = iv_now;
  timespec_add_msec(&self->reap_timer.expires, self->owner->time_reap * 1000 / 2);
  iv_timer_register(&self->reap_timer);
}

static void
affile_dw_reap(gpointer s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  main_loop_assert_main_thread();

  g_static_mutex_lock(&self->lock);
  if (!log_writer_has_pending_writes((LogWriter *) self->writer) &&
      !self->queue_pending &&
      (cached_g_current_time_sec() - self->last_msg_stamp) >= self->owner->time_reap)
    {
      g_static_mutex_unlock(&self->lock);
      msg_verbose("Destination timed out, reaping",
                  evt_tag_str("template", self->owner->filename_template->template),
                  evt_tag_str("filename", self->filename),
                  NULL);
      affile_dd_reap_writer(self->owner, self);
    }
  else
    {
      g_static_mutex_unlock(&self->lock);
      affile_dw_arm_reaper(self);
    }
}

static gboolean
affile_dw_reopen(AFFileDestWriter *self)
{
  int fd, flags;
  struct stat st;

  self->last_open_stamp = self->last_msg_stamp;
  if (self->owner->overwrite_if_older > 0 && 
      stat(self->filename, &st) == 0 &&
      st.st_mtime < time(NULL) - self->owner->overwrite_if_older)
    {
      msg_info("Destination file is older than overwrite_if_older(), overwriting",
                 evt_tag_str("filename", self->filename),
                 evt_tag_int("overwrite_if_older", self->owner->overwrite_if_older),
                 NULL);
      unlink(self->filename);
    }

  if (self->owner->flags & AFFILE_PIPE)
    flags = O_RDWR | O_NOCTTY | O_NONBLOCK | O_LARGEFILE;
  else
    flags = O_WRONLY | O_CREAT | O_NOCTTY | O_NONBLOCK | O_LARGEFILE;


  if (affile_open_file(self->filename, flags,
                       self->owner->file_uid, self->owner->file_gid, self->owner->file_perm, 
                       self->owner->dir_uid, self->owner->dir_gid, self->owner->dir_perm, 
                       !!(self->owner->flags & AFFILE_CREATE_DIRS), FALSE, !!(self->owner->flags & AFFILE_PIPE), &fd))
    {
      guint write_flags;
      
      write_flags =
        ((self->owner->flags & AFFILE_PIPE) ? LTF_PIPE : LTF_APPEND) |
        ((self->owner->flags & AFFILE_FSYNC) ? LTF_FSYNC : 0);
      log_writer_reopen(self->writer,
                        self->owner->flags & AFFILE_PIPE
                        ? log_proto_text_client_new(log_transport_plain_new(fd, write_flags))
                        : log_proto_file_writer_new(log_transport_plain_new(fd, write_flags), self->owner->writer_options.flush_lines));

      main_loop_call((void * (*)(void *)) affile_dw_arm_reaper, self, TRUE);
    }
  else
    {
      msg_error("Error opening file for writing",
                evt_tag_str("filename", self->filename),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return self->owner->super.super.optional;
    }
  return TRUE;
}

static gboolean
affile_dw_init(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (cfg)
    self->time_reopen = cfg->time_reopen;

  msg_verbose("Initializing destination file writer",
              evt_tag_str("template", self->owner->filename_template->template),
              evt_tag_str("filename", self->filename),
              NULL);

  if (!self->writer)
    {
      guint32 flags;

      flags = LW_FORMAT_FILE |
        ((self->owner->flags & AFFILE_PIPE) ? 0 : LW_SOFT_FLOW_CONTROL);

      self->writer = log_writer_new(flags);
    }
  log_writer_set_options((LogWriter *) self->writer, s, &self->owner->writer_options, 1,
                         self->owner->flags & AFFILE_PIPE ? SCS_PIPE : SCS_FILE,
                         self->owner->super.super.id, self->filename);
  log_writer_set_queue(self->writer, log_dest_driver_acquire_queue(&self->owner->super, NULL));

  if (!log_pipe_init(self->writer, NULL))
    {
      msg_error("Error initializing log writer", NULL);
      log_pipe_unref(self->writer);
      self->writer = NULL;
      return FALSE;
    }
  log_pipe_append(&self->super, self->writer);

  return affile_dw_reopen(self);
}

static gboolean
affile_dw_deinit(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  main_loop_assert_main_thread();
  if (self->writer)
    {
      log_pipe_deinit(self->writer);
    }
  if (iv_timer_registered(&self->reap_timer))
    iv_timer_unregister(&self->reap_timer);
  return TRUE;
}

/*
 * NOTE: the caller (e.g. AFFileDestDriver) holds a reference to @self, thus
 * @self may _never_ be freed, even if the reaper timer is elapsed in the
 * main thread.
 */
static void
affile_dw_queue(LogPipe *s, LogMessage *lm, const LogPathOptions *path_options, gpointer user_data)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  g_static_mutex_lock(&self->lock);
  self->last_msg_stamp = cached_g_current_time_sec();
  if (self->last_open_stamp == 0)
    self->last_open_stamp = self->last_msg_stamp;

  if (!log_writer_opened((LogWriter *) self->writer) &&
      !self->reopen_pending &&
      (self->last_open_stamp < self->last_msg_stamp - self->time_reopen))
    {
      self->reopen_pending = TRUE;
      /* if the file couldn't be opened, try it again every time_reopen seconds */
      g_static_mutex_unlock(&self->lock);
      affile_dw_reopen(self);
      g_static_mutex_lock(&self->lock);
      self->reopen_pending = FALSE;
    }
  g_static_mutex_unlock(&self->lock);

  log_pipe_forward_msg(&self->super, lm, path_options);
}

static void
affile_dw_set_owner(AFFileDestWriter *self, AFFileDestDriver *owner)
{
  if (self->owner)
    log_pipe_unref(&self->owner->super.super.super);
  log_pipe_ref(&owner->super.super.super);
  self->owner = owner;
  if (self->writer)
    log_writer_set_options((LogWriter *) self->writer, &self->super, &owner->writer_options, 1, SCS_FILE, self->owner->super.super.id, self->filename);
  
}

static void
affile_dw_free(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;
  
  log_pipe_unref(self->writer);
  self->writer = NULL;
  g_free(self->filename);
  log_pipe_unref(&self->owner->super.super.super);
  log_pipe_free_method(s);
}

static AFFileDestWriter *
affile_dw_new(AFFileDestDriver *owner, const gchar *filename)
{
  AFFileDestWriter *self = g_new0(AFFileDestWriter, 1);
  
  log_pipe_init_instance(&self->super);

  self->super.init = affile_dw_init;
  self->super.deinit = affile_dw_deinit;
  self->super.free_fn = affile_dw_free;  
  self->super.queue = affile_dw_queue;
  log_pipe_ref(&owner->super.super.super);
  self->owner = owner;
  self->time_reopen = 60;

  IV_TIMER_INIT(&self->reap_timer);
  self->reap_timer.cookie = self;
  self->reap_timer.handler = affile_dw_reap;

  /* we have to take care about freeing filename later. 
     This avoids a move of the filename. */
  self->filename = g_strdup(filename);
  g_static_mutex_init(&self->lock);
  return self;
}

void 
affile_dd_set_file_uid(LogDriver *s, const gchar *file_uid)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->file_uid = 0;
  if (!resolve_user(file_uid, &self->file_uid))
    {
      msg_error("Error resolving user",
                 evt_tag_str("user", file_uid),
                 NULL);
    }
}

void 
affile_dd_set_file_gid(LogDriver *s, const gchar *file_gid)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->file_gid = 0;
  if (!resolve_group(file_gid, &self->file_gid))
    {
      msg_error("Error resolving group",
                 evt_tag_str("group", file_gid),
                 NULL);
    }
}

void 
affile_dd_set_file_perm(LogDriver *s, gint file_perm)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->file_perm = file_perm;
}

void 
affile_dd_set_dir_uid(LogDriver *s, const gchar *dir_uid)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->dir_uid = 0;
  if (!resolve_user(dir_uid, &self->dir_uid))
    {
      msg_error("Error resolving user",
                 evt_tag_str("user", dir_uid),
                 NULL);
    }
}

void 
affile_dd_set_dir_gid(LogDriver *s, const gchar *dir_gid)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->dir_gid = 0;
  if (!resolve_group(dir_gid, &self->dir_gid))
    {
      msg_error("Error resolving group",
                 evt_tag_str("group", dir_gid),
                 NULL);
    }
}

void 
affile_dd_set_dir_perm(LogDriver *s, gint dir_perm)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->dir_perm = dir_perm;
}

void 
affile_dd_set_create_dirs(LogDriver *s, gboolean create_dirs)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  if (create_dirs)
    self->flags |= AFFILE_CREATE_DIRS;
  else 
    self->flags &= ~AFFILE_CREATE_DIRS;
}

void 
affile_dd_set_overwrite_if_older(LogDriver *s, gint overwrite_if_older)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->overwrite_if_older = overwrite_if_older;
}

void 
affile_dd_set_fsync(LogDriver *s, gboolean fsync)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  if (fsync)
    self->flags |= AFFILE_FSYNC;
  else
    self->flags &= ~AFFILE_FSYNC;
}

void
affile_dd_set_local_time_zone(LogDriver *s, const gchar *local_time_zone)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  self->local_time_zone = g_strdup(local_time_zone);
}

static inline gchar *
affile_dd_format_persist_name(AFFileDestDriver *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name), "affile_dd_writers(%s)", self->filename_template->template);
  return persist_name;
}

static void
affile_dd_reap_writer(AFFileDestDriver *self, AFFileDestWriter *dw)
{
  main_loop_assert_main_thread();
  
  if ((self->flags & AFFILE_NO_EXPAND) == 0)
    {
      g_static_mutex_lock(&self->lock);
      /* remove from hash table */
      g_hash_table_remove(self->writer_hash, dw->filename);
      g_static_mutex_unlock(&self->lock);
    }
  else
    {
      g_static_mutex_lock(&self->lock);
      g_assert(dw == self->single_writer);
      self->single_writer = NULL;
      g_static_mutex_unlock(&self->lock);
    }

  log_pipe_deinit(&dw->super);
  log_pipe_unref(&dw->super);
}


/**
 * affile_dd_reuse_writer:
 *
 * This function is called as a g_hash_table_foreach() callback to set the
 * owner of each writer, previously connected to an AFileDestDriver instance
 * in an earlier configuration. This way AFFileDestWriter instances are
 * remembered accross reloads.
 * 
 **/
static void
affile_dd_reuse_writer(gpointer key, gpointer value, gpointer user_data)
{
  AFFileDestDriver *self = (AFFileDestDriver *) user_data;
  AFFileDestWriter *writer = (AFFileDestWriter *) value;
  
  affile_dw_set_owner(writer, self);
  log_pipe_init(&writer->super, NULL);
}


static gboolean
affile_dd_init(LogPipe *s)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_dest_driver_init_method(s))
    return FALSE;

  if (cfg->create_dirs)
    self->flags |= AFFILE_CREATE_DIRS;
  if (self->file_uid == -1)
    self->file_uid = cfg->file_uid;
  if (self->file_gid == -1)
    self->file_gid = cfg->file_gid;
  if (self->file_perm == -1)
    self->file_perm = cfg->file_perm;
  if (self->dir_uid == -1)
    self->dir_uid = cfg->dir_uid;
  if (self->dir_gid == -1)
    self->dir_gid = cfg->dir_gid;
  if (self->dir_perm == -1)
    self->dir_perm = cfg->dir_perm;
  if (self->time_reap == -1)
    self->time_reap = cfg->time_reap;
  
  log_writer_options_init(&self->writer_options, cfg, 0);
  log_template_options_init(&self->template_fname_options, cfg);
              
  if ((self->flags & AFFILE_NO_EXPAND) == 0)
    {
      self->writer_hash = cfg_persist_config_fetch(cfg, affile_dd_format_persist_name(self));
      if (self->writer_hash)
        g_hash_table_foreach(self->writer_hash, affile_dd_reuse_writer, self);
    }
  else
    {
      self->single_writer = cfg_persist_config_fetch(cfg, affile_dd_format_persist_name(self));
      if (self->single_writer)
        {
          affile_dw_set_owner(self->single_writer, self);
          log_pipe_init(&self->single_writer->super, cfg);
        }
    }
  
  
  return TRUE;
}


/**
 * This is registered as a destroy-notify callback for an AFFileDestWriter
 * instance. It destructs and frees the writer instance.
 **/
static void
affile_dd_destroy_writer(gpointer value)
{
  AFFileDestWriter *writer = (AFFileDestWriter *) value;

  main_loop_assert_main_thread();
  log_pipe_deinit(&writer->super);
  log_pipe_unref(&writer->super);
}

/*
 * This function is called as a g_hash_table_foreach_remove() callback to
 * free the specific AFFileDestWriter instance in the hashtable.
 */
static gboolean
affile_dd_destroy_writer_hr(gpointer key, gpointer value, gpointer user_data)
{
  affile_dd_destroy_writer(value);
  return TRUE;
}

/**
 * affile_dd_destroy_writer_hash:
 * @value: GHashTable instance passed as a generic pointer
 *
 * Destroy notify callback for the GHashTable storing AFFileDestWriter instances.
 **/
static void
affile_dd_destroy_writer_hash(gpointer value)
{
  GHashTable *writer_hash = (GHashTable *) value;
  
  g_hash_table_foreach_remove(writer_hash, affile_dd_destroy_writer_hr, NULL);
  g_hash_table_destroy(writer_hash);
}

static void
affile_dd_deinit_writer(gpointer key, gpointer value, gpointer user_data)
{
  log_pipe_deinit((LogPipe *) value);
}

static gboolean
affile_dd_deinit(LogPipe *s)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  /* NOTE: we free all AFFileDestWriter instances here as otherwise we'd
   * have circular references between AFFileDestDriver and file writers */
  if (self->single_writer)
    {
      g_assert(self->writer_hash == NULL);

      log_pipe_deinit(&self->single_writer->super);
      cfg_persist_config_add(cfg, affile_dd_format_persist_name(self), self->single_writer, affile_dd_destroy_writer, FALSE);
      self->single_writer = NULL;
    }
  else if (self->writer_hash)
    {
      g_assert(self->single_writer == NULL);
      
      g_hash_table_foreach(self->writer_hash, affile_dd_deinit_writer, NULL);
      cfg_persist_config_add(cfg, affile_dd_format_persist_name(self), self->writer_hash, affile_dd_destroy_writer_hash, FALSE);
      self->writer_hash = NULL;
    }

  if (!log_dest_driver_deinit_method(s))
    return FALSE;

  return TRUE;
}

/*
 * This function is ran in the main thread whenever a writer is not yet
 * instantiated.  Returns a reference to the newly constructed LogPipe
 * instance where the caller needs to forward its message.
 */
static LogPipe *
affile_dd_open_writer(gpointer args[])
{
  AFFileDestDriver *self = args[0];
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);
  AFFileDestWriter *next;

  main_loop_assert_main_thread();
  if (self->flags & AFFILE_NO_EXPAND)
    {
      if (!self->single_writer)
	{
	  next = affile_dw_new(self, self->filename_template->template);
          if (next && log_pipe_init(&next->super, cfg))
	    {
	      log_pipe_ref(&next->super);
	      g_static_mutex_lock(&self->lock);
	      self->single_writer = next;
	      g_static_mutex_unlock(&self->lock);
	    }
	  else
	    {
	      log_pipe_unref(&next->super);
	      next = NULL;
	    }
	}
      else
        {
          next = self->single_writer;
          log_pipe_ref(&next->super);
        }
    }
  else
    {
      GString *filename = args[1];

      /* hash table construction is serialized, as we only do that in the main thread. */
      if (!self->writer_hash)
        self->writer_hash = g_hash_table_new(g_str_hash, g_str_equal);

      /* we don't need to lock the hashtable as it is only written in
       * the main thread, which we're running right now.  lookups in
       * other threads must be locked. writers must be locked even in
       * this thread to exclude lookups in other threads.  */

      next = g_hash_table_lookup(self->writer_hash, filename->str);
      if (!next)
	{
	  next = affile_dw_new(self, filename->str);
          if (!log_pipe_init(&next->super, cfg))
	    {
	      log_pipe_unref(&next->super);
	      next = NULL;
	    }
	  else
	    {
	      log_pipe_ref(&next->super);
	      g_static_mutex_lock(&self->lock);
              g_hash_table_insert(self->writer_hash, next->filename, next);
              g_static_mutex_unlock(&self->lock);
            }
        }
      else
        {
          log_pipe_ref(&next->super);
        }
    }

  if (next)
    {
      next->queue_pending = TRUE;
      /* we're returning a reference */
      return &next->super;
    }
  return NULL;
}

static void
affile_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  AFFileDestWriter *next;
  gpointer args[2] = { self, NULL };

  if (self->flags & AFFILE_NO_EXPAND)
    {
      /* no need to lock the check below, the worst case that happens is
       * that we go to the mainloop to return the same information, but this
       * is not fast path anyway */

      g_static_mutex_lock(&self->lock);
      if (!self->single_writer)
        {
          g_static_mutex_unlock(&self->lock);
          next = main_loop_call((void *(*)(void *)) affile_dd_open_writer, args, TRUE);
        }
      else
        {
          /* we need to lock single_writer in order to get a reference and
           * make sure it is not a stale pointer by the time we ref it */
          next = self->single_writer;
          next->queue_pending = TRUE;
          log_pipe_ref(&next->super);
          g_static_mutex_unlock(&self->lock);
        }
    }
  else
    {
      GString *filename;

      filename = g_string_sized_new(32);
      log_template_format(self->filename_template, msg, &self->template_fname_options, LTZ_LOCAL, 0, NULL, filename);

      g_static_mutex_lock(&self->lock);
      if (self->writer_hash)
        next = g_hash_table_lookup(self->writer_hash, filename->str);
      else
        next = NULL;

      if (next)
        {
          log_pipe_ref(&next->super);
          next->queue_pending = TRUE;
          g_static_mutex_unlock(&self->lock);
        }
      else
	{
	  g_static_mutex_unlock(&self->lock);
	  args[1] = filename;
	  next = main_loop_call((void *(*)(void *)) affile_dd_open_writer, args, TRUE);
	}
      g_string_free(filename, TRUE);
    }
  if (next)
    {
      log_pipe_queue(&next->super, msg, path_options);
      next->queue_pending = FALSE;
      log_pipe_unref(&next->super);
    }
  else
    log_msg_drop(msg, path_options);
}

static void
affile_dd_free(LogPipe *s)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  /* NOTE: this must be NULL as deinit has freed it, otherwise we'd have circular references */
  g_assert(self->single_writer == NULL && self->writer_hash == NULL);

  log_template_options_destroy(&self->template_fname_options);
  log_template_unref(self->filename_template);
  log_writer_options_destroy(&self->writer_options);
  log_dest_driver_free(s);
}

LogDriver *
affile_dd_new(gchar *filename, guint32 flags)
{
  AFFileDestDriver *self = g_new0(AFFileDestDriver, 1);

  log_dest_driver_init_instance(&self->super);
  self->super.super.super.init = affile_dd_init;
  self->super.super.super.deinit = affile_dd_deinit;
  self->super.super.super.queue = affile_dd_queue;
  self->super.super.super.free_fn = affile_dd_free;
  self->filename_template = log_template_new(configuration, NULL);
  log_template_compile(self->filename_template, filename, NULL);
  self->flags = flags;
  self->file_uid = self->file_gid = -1;
  self->file_perm = -1;
  self->dir_uid = self->dir_gid = -1;
  self->dir_perm = -1;
  log_writer_options_defaults(&self->writer_options);
  if (strchr(filename, '$') == NULL)
    {
      self->flags |= AFFILE_NO_EXPAND;
    }
  self->time_reap = -1;
  log_template_options_defaults(&self->template_fname_options);
  g_static_mutex_init(&self->lock);
  return &self->super.super;
}
