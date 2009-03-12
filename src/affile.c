/*
 * Copyright (c) 2002-2009 BalaBit IT Ltd, Budapest, Hungary
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
#include "affile.h"
#include "driver.h"
#include "messages.h"
#include "misc.h"
#include "serialize.h"
#include "gprocess.h"
#include "stats.h"

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
                 uid_t uid, gid_t gid, mode_t mode,
                 uid_t dir_uid, gid_t dir_gid, mode_t dir_mode,
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

  if (create_dirs && !create_containing_directory(name, dir_uid, dir_gid, dir_mode))
    return FALSE;

  if (privileged)
    {
      saved_caps = g_process_cap_save();
      g_process_cap_modify(CAP_DAC_READ_SEARCH, TRUE);
      g_process_cap_modify(CAP_SYS_ADMIN, TRUE);
    }
  if (stat(name, &st) >= 0)
    {
      if (is_pipe && !S_ISFIFO(st.st_mode))
        {
          msg_error("Error opening pipe, underlying file is not a FIFO, it should be used by file()",
                    evt_tag_str("filename", name),
                    NULL);
          goto exit;
        }
      else if (!is_pipe && S_ISFIFO(st.st_mode))
        {
          msg_error("Error opening file, underlying file is a FIFO, it should be used by pipe()",
                    evt_tag_str("filename", name),
                    NULL);
          goto exit;
        }
    }
  *fd = open(name, flags, mode);
  if (is_pipe && *fd < 0 && errno == ENOENT)
    {
      if (mkfifo(name, 0666) >= 0)
        *fd = open(name, flags, 0666);
    }

  if (*fd != -1)
    {
      g_fd_set_cloexec(*fd, TRUE);
      
      g_process_cap_modify(CAP_DAC_OVERRIDE, TRUE);
      if (uid != (uid_t) -1)
        fchown(*fd, uid, -1);
      if (gid != (gid_t) -1)
        fchown(*fd, -1, gid);
      if (mode != (mode_t) -1)
        fchmod(*fd, mode);
    }
 exit:
  if (privileged)
    {
      g_process_cap_restore(saved_caps);
    }
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
affile_sd_save_pos(LogPipe *s, GlobalConfig *cfg)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  if (cfg->persist && (self->flags & AFFILE_PIPE) == 0)
    {
      SerializeArchive *archive;
      GString *str = g_string_sized_new(0);
  
      archive = serialize_string_archive_new(str);
      log_reader_save_state((LogReader *) self->reader, archive);
      cfg_persist_config_add_survivor(cfg, affile_sd_format_persist_name(self), str->str,  str->len, TRUE);
      serialize_archive_free(archive);
      g_string_free(str, TRUE);
    }
}

static void
affile_sd_load_pos(LogPipe *s, GlobalConfig *cfg)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  gchar *str;
  off_t cur_pos;
  gsize str_len;
  gint version;
  
  if ((self->flags & AFFILE_PIPE) || !cfg->persist)
    return;
     
  str = cfg_persist_config_fetch(cfg, affile_sd_format_persist_name(self), &str_len, &version);
  if (!str)
    return;

  if (version == 2)
    {
      /* NOTE: legacy, should be removed once the release after 3.0 is published */
      cur_pos = strtoll(str, NULL, 10);
      log_reader_update_pos((LogReader *) self->reader, cur_pos);
      g_free(str);
    }
  else if (version >= 3)
    {
      GString *g_str = g_string_new("");
      SerializeArchive *archive;

      g_str = g_string_assign_len(g_str, str, str_len);
      archive = serialize_string_archive_new(g_str);
      
      log_reader_restore_state((LogReader *) self->reader, archive);

      serialize_archive_free(archive);
      g_string_free(g_str, TRUE);
      g_free(str);
      
    }
}

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
            
            transport = log_transport_plain_new(fd, 0);
            transport->timeout = 10;

            self->reader = log_reader_new(
                                  log_proto_plain_new_server(transport, self->reader_options.padding,
                                                             self->reader_options.msg_size,
                                                             ((self->reader_options.follow_freq > 0)
                                                                    ? LPPF_IGNORE_EOF
                                                                    : LPPF_NOMREAD)
                                                            ),
                                  LR_LOCAL);

            log_reader_set_options(self->reader, s, &self->reader_options, 1, SCS_FILE, self->super.id, self->filename->str);

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
affile_sd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  
  log_msg_add_dyn_value(msg, "FILE_NAME", self->filename->str);

  log_pipe_forward_msg(s, msg, path_options);
}

static gboolean
affile_sd_init(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);
  gint fd;
  gboolean file_opened, open_deferred = FALSE;

  log_reader_options_init(&self->reader_options, cfg, self->super.group);

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

      transport = log_transport_plain_new(fd, 0);
      transport->timeout = 10;

      /* FIXME: we shouldn't use reader_options to store log protocol parameters */
      self->reader = log_reader_new(
                            log_proto_plain_new_server(transport, self->reader_options.padding,
                                                       self->reader_options.msg_size,
                                                       ((self->reader_options.follow_freq > 0)
                                                            ? LPPF_IGNORE_EOF
                                                            : LPPF_NOMREAD)
                                                      ),
                            LR_LOCAL);
      log_reader_set_options(self->reader, s, &self->reader_options, 1, SCS_FILE, self->super.id, self->filename->str);

      log_reader_set_follow_filename(self->reader, self->filename->str);

      /* NOTE: if the file could not be opened, we ignore the last
       * remembered file position, if the file is created in the future
       * we're going to read from the start. */
      
      affile_sd_load_pos(s, cfg);
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
    }
  else
    {
      msg_error("Error opening file for reading",
                evt_tag_str("filename", self->filename->str),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return self->super.optional;
    }
  return TRUE;

}

static gboolean
affile_sd_deinit(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (self->reader)
    {
      affile_sd_save_pos(s, cfg);
      log_pipe_deinit(self->reader);
      log_pipe_unref(self->reader);
      self->reader = NULL;
    }

  return TRUE;
}

static void
affile_sd_free(LogPipe *s)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  g_string_free(self->filename, TRUE);
  g_assert(!self->reader);

  log_reader_options_destroy(&self->reader_options);

  log_drv_free(s);
}

LogDriver *
affile_sd_new(gchar *filename, guint32 flags)
{
  AFFileSourceDriver *self = g_new0(AFFileSourceDriver, 1);
  
  log_drv_init_instance(&self->super);
  self->filename = g_string_new(filename);
  self->flags = flags;
  self->super.super.init = affile_sd_init;
  self->super.super.queue = affile_sd_queue;
  self->super.super.deinit = affile_sd_deinit;
  self->super.super.notify = affile_sd_notify;
  self->super.super.free_fn = affile_sd_free;
  log_reader_options_defaults(&self->reader_options);
  
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
        self->reader_options.follow_freq = 1;
        
#if __linux__
      if (strcmp(filename, "/proc/kmsg") == 0)
        {
          self->reader_options.follow_freq = 0;
        }
#endif
    }
#if __linux__
  if (strcmp(filename, "/proc/kmsg") == 0)
    {
      self->flags |= AFFILE_PRIVILEGED;
    }
#endif
  return &self->super;
}

struct _AFFileDestWriter
{
  LogPipe super;
  AFFileDestDriver *owner;
  GString *filename;
  LogPipe *writer;
  time_t last_msg_stamp;
  time_t last_open_stamp;
  time_t time_reopen;
};

static gboolean
affile_dw_reapable(AFFileDestWriter *self)
{
  return log_queue_get_length(((LogWriter *) self->writer)->queue) == 0;
}

static gboolean
affile_dw_init(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;
  int fd, flags;
  struct stat st;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (cfg)
    self->time_reopen = cfg->time_reopen;

  msg_verbose("Initializing destination file writer",
              evt_tag_str("template", self->owner->filename_template->template),
              evt_tag_str("filename", self->filename->str),
              NULL);
              
  if (self->owner->overwrite_if_older > 0 && 
      stat(self->filename->str, &st) == 0 && 
      st.st_mtime < time(NULL) - self->owner->overwrite_if_older)
    {
      msg_info("Destination file is older than overwrite_if_older(), overwriting",
                 evt_tag_str("filename", self->filename->str),
                 evt_tag_int("overwrite_if_older", self->owner->overwrite_if_older),
                 NULL);
      unlink(self->filename->str);
    }

  if (self->owner->flags & AFFILE_PIPE)
    flags = O_RDWR | O_NOCTTY | O_NONBLOCK | O_LARGEFILE;
  else
    flags = O_WRONLY | O_CREAT | O_NOCTTY | O_NONBLOCK | O_LARGEFILE;

  self->last_open_stamp = time(NULL);
  if (affile_open_file(self->filename->str, flags, 
                       self->owner->file_uid, self->owner->file_gid, self->owner->file_perm, 
                       self->owner->dir_uid, self->owner->dir_gid, self->owner->dir_perm, 
                       !!(self->owner->flags & AFFILE_CREATE_DIRS), FALSE, !!(self->owner->flags & AFFILE_PIPE), &fd))
    {
      guint write_flags;
      
      if (!self->writer)
        {
          self->writer = log_writer_new(LW_FORMAT_FILE | ((self->owner->flags & AFFILE_PIPE) ? 0 : LW_ALWAYS_WRITABLE));
          log_writer_set_options((LogWriter *) self->writer, s, &self->owner->writer_options, 1, self->owner->flags & AFFILE_PIPE ? SCS_PIPE : SCS_FILE, self->owner->super.id, self->filename->str);
          log_pipe_append(&self->super, self->writer);
        }
      if (!log_pipe_init(self->writer, NULL))
        {
          msg_error("Error initializing log writer", NULL);
          log_pipe_unref(self->writer);
          self->writer = NULL;
          close(fd);
          return FALSE;
        }
      write_flags = ((self->owner->flags & AFFILE_FSYNC) ? LTF_FSYNC : 0) | LTF_APPEND;
      log_writer_reopen(self->writer, log_proto_plain_new_client(log_transport_plain_new(fd, write_flags)));
    }
  else
    {
      msg_error("Error opening file for writing",
                evt_tag_str("filename", self->filename->str),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return self->owner->super.optional;
    }
  return TRUE;
}

static gboolean
affile_dw_deinit(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  if (self->writer)
    {
      log_pipe_deinit(self->writer);
    }
  return TRUE;
}

static void
affile_dw_queue(LogPipe *s, LogMessage *lm, const LogPathOptions *path_options)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  self->last_msg_stamp = time(NULL);
  if (!s->pipe_next && self->last_open_stamp < self->last_msg_stamp - self->time_reopen)
    {
      log_pipe_init(&self->super, NULL);
    }
    
  if (s->pipe_next)
    log_pipe_forward_msg(s, lm, path_options);
  else
    log_msg_drop(lm, path_options);
}

static void
affile_dw_set_owner(AFFileDestWriter *self, AFFileDestDriver *owner)
{
  if (self->owner)
    log_pipe_unref(&self->owner->super.super);
  log_pipe_ref(&owner->super.super);
  self->owner = owner;
  if (self->writer)
    log_writer_set_options((LogWriter *) self->writer, &self->super, &owner->writer_options, 1, SCS_FILE, self->owner->super.id, self->filename->str);
  
}

static void
affile_dw_free(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;
  
  log_pipe_unref(self->writer);
  self->writer = NULL;
  g_string_free(self->filename, TRUE);
  log_pipe_unref(&self->owner->super.super);
  log_pipe_free(s);
}

static AFFileDestWriter *
affile_dw_new(AFFileDestDriver *owner, GString *filename)
{
  AFFileDestWriter *self = g_new0(AFFileDestWriter, 1);
  
  log_pipe_init_instance(&self->super);

  self->super.init = affile_dw_init;
  self->super.deinit = affile_dw_deinit;
  self->super.free_fn = affile_dw_free;  
  self->super.queue = affile_dw_queue;
  log_pipe_ref(&owner->super.super);
  self->owner = owner;
  self->time_reopen = 60;
  
  /* we have to take care about freeing filename later. 
     This avoids a move of the filename. */
  self->filename = filename;
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
affile_dd_set_file_perm(LogDriver *s, mode_t file_perm)
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
affile_dd_set_dir_perm(LogDriver *s, mode_t dir_perm)
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

static inline gchar *
affile_dd_format_persist_name(AFFileDestDriver *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name), "affile_dd_writers(%s)", self->filename_template->template);
  return persist_name;
}

static time_t reap_now = 0;

static gboolean
affile_dd_reap_writers(gpointer key, gpointer value, gpointer user_data)
{
  AFFileDestDriver *self = (AFFileDestDriver *) user_data;
  AFFileDestWriter *dw = (AFFileDestWriter *) value;
  
  if ((reap_now - dw->last_msg_stamp) >= self->time_reap && affile_dw_reapable(dw))
    {
      msg_verbose("Destination timed out, reaping", 
                  evt_tag_str("template", self->filename_template->template),
                  evt_tag_str("filename", dw->filename->str),
                  NULL);
      log_pipe_deinit(&dw->super);
      log_pipe_unref(&dw->super);
      
      /* remove from hash table */
      return TRUE;
    }
  return FALSE;
}

static gboolean
affile_dd_reap(gpointer s)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  msg_verbose("Reaping unused destination files",
              evt_tag_str("template", self->filename_template->template),
              NULL);  
  reap_now = time(NULL);
  if (self->writer_hash)
    g_hash_table_foreach_remove(self->writer_hash, affile_dd_reap_writers, self);
  return TRUE;
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

  if (cfg->create_dirs)
    self->flags |= AFFILE_CREATE_DIRS;
  if (self->file_uid == -1)
    self->file_uid = cfg->file_uid;
  if (self->file_gid == -1)
    self->file_gid = cfg->file_gid;
  if (self->file_perm == (mode_t) -1)
    self->file_perm = cfg->file_perm;
  if (self->dir_uid == -1)
    self->dir_uid = cfg->dir_uid;
  if (self->dir_gid == -1)
    self->dir_gid = cfg->dir_gid;
  if (self->dir_perm == (mode_t) -1)
    self->dir_perm = cfg->dir_perm;
  if (self->time_reap == -1)
    self->time_reap = cfg->time_reap;
  
  self->use_time_recvd = cfg->use_time_recvd;
  
  log_writer_options_init(&self->writer_options, cfg, 0);
              
  
  if ((self->flags & AFFILE_NO_EXPAND) == 0)
    {
      self->reap_timer = g_timeout_add_full(G_PRIORITY_DEFAULT, self->time_reap * 1000 / 2, affile_dd_reap, self, NULL);
      self->writer_hash = cfg_persist_config_fetch(cfg, affile_dd_format_persist_name(self), NULL, NULL);
      if (self->writer_hash)
        g_hash_table_foreach(self->writer_hash, affile_dd_reuse_writer, self);
    }
  else
    {
      self->single_writer = cfg_persist_config_fetch(cfg, affile_dd_format_persist_name(self), NULL, NULL);
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
      cfg_persist_config_add(cfg, affile_dd_format_persist_name(self), self->single_writer, -1, affile_dd_destroy_writer, FALSE);
      self->single_writer = NULL;
    }
  else if (self->writer_hash)
    {
      g_assert(self->single_writer == NULL);
      
      g_hash_table_foreach(self->writer_hash, affile_dd_deinit_writer, NULL);
      cfg_persist_config_add(cfg, affile_dd_format_persist_name(self), self->writer_hash, -1, affile_dd_destroy_writer_hash, FALSE);
      self->writer_hash = NULL;
    }
  if (self->reap_timer)
    g_source_remove(self->reap_timer);
  return TRUE;
}

static void
affile_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  AFFileDestWriter *next;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (self->flags & AFFILE_NO_EXPAND)
    {
      if (!self->single_writer)
	{
	  next = affile_dw_new(self, g_string_new(self->filename_template->template));
          if (next && log_pipe_init(&next->super, cfg))
	    {
	      self->single_writer = next;
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
        }
    }
  else
    {
      GString *filename;

      if (!self->writer_hash)
	self->writer_hash = g_hash_table_new(g_str_hash, g_str_equal);

      filename = g_string_sized_new(32);
      log_template_format(self->filename_template, msg, 
		    ((self->flags & AFFILE_TMPL_ESCAPE) ? LT_ESCAPE : 0) |
		    (self->use_time_recvd ? LT_STAMP_RECVD : 0),
		    TS_FMT_BSD,
		    NULL,
		    0,
		    0,
		    filename);
      next = g_hash_table_lookup(self->writer_hash, filename->str);
      if (!next)
	{
	  next = affile_dw_new(self, filename);
          if (!log_pipe_init(&next->super, cfg))
	    {
	      log_pipe_unref(&next->super);
	      next = NULL;
	    }
	  else
	    g_hash_table_insert(self->writer_hash, filename->str, next);
	}
      else
        g_string_free(filename, TRUE);
    }
  if (next)
    log_pipe_queue(&next->super, msg, path_options);
  else
    log_msg_drop(msg, path_options);
}

static void
affile_dd_free(LogPipe *s)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  /* NOTE: this must be NULL as deinit has freed it, otherwise we'd have circular references */
  g_assert(self->single_writer == NULL && self->writer_hash == NULL);

  log_template_unref(self->filename_template);
  log_writer_options_destroy(&self->writer_options);
  log_drv_free(s);
}

LogDriver *
affile_dd_new(gchar *filename, guint32 flags)
{
  AFFileDestDriver *self = g_new0(AFFileDestDriver, 1);

  log_drv_init_instance(&self->super);
  self->super.super.init = affile_dd_init;
  self->super.super.deinit = affile_dd_deinit;
  self->super.super.queue = affile_dd_queue;
  self->super.super.free_fn = affile_dd_free;
  self->filename_template = log_template_new(NULL, filename);
  self->flags = flags;
  self->file_uid = self->file_gid = -1;
  self->file_perm = (mode_t) -1;
  self->dir_uid = self->dir_gid = -1;
  self->dir_perm = (mode_t) -1;
  log_writer_options_defaults(&self->writer_options);
  if (strchr(filename, '$') == NULL)
    {
      self->flags |= AFFILE_NO_EXPAND;
    }
  self->time_reap = -1;
  return &self->super;
}
