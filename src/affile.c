/*
 * Copyright (c) 2002-2007 BalaBit IT Ltd, Budapest, Hungary
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
#include "macros.h"
#include "misc.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

#if !HAVE_O_LARGEFILE
#define O_LARGEFILE 0
#endif

static gboolean
affile_open_file(gchar *name, int flags,
	     int uid, int gid, int mode,
	     int dir_uid, int dir_gid, int dir_mode,
	     int create_dirs, int *fd)
{
  if (strstr(name, "../") || strstr(name, "/..")) 
    {
      msg_error("Spurious path, logfile not created",
                evt_tag_str("path", name),
                NULL);
      return FALSE;
    }

  *fd = open(name, flags, mode);
  if (create_dirs && *fd == -1 && errno == ENOENT) 
    {
      /* directory does not exist */
      char *p = name + 1;
      
      p = strchr(p, '/');
      while (p) 
	{
	  struct stat st;
	  *p = 0;
	  if (stat(name, &st) == 0) 
	    {
	      if (!S_ISDIR(st.st_mode))
		return FALSE;
	    }
	  else if (errno == ENOENT) 
	    {
	      if (mkdir(name, dir_mode) == -1)
		return 0;
	      if (dir_uid != -1 || dir_gid != -1)
		chown(name, dir_uid, dir_gid);
	      if (dir_mode != -1)
		chmod(name, dir_mode);
	    }
	  *p = '/';
	  p = strchr(p + 1, '/');
	}
      *fd = open(name, flags, mode);
    }
  if (*fd != -1)
    {
      g_fd_set_cloexec(*fd, TRUE);
      if (uid != -1)
        fchown(*fd, uid, -1);
      if (gid != -1)
        fchown(*fd, -1, gid);
      if (mode != -1)
        fchmod(*fd, mode);
    }
  return *fd != -1;
}

static gboolean
affile_sd_open_file(AFFileSourceDriver *self, gint *fd)
{
  gint flags;
  
  if (self->flags & AFFILE_PIPE)
    flags = O_RDWR | O_NOCTTY | O_NONBLOCK | O_LARGEFILE;
  else
    flags = O_RDONLY | O_NOCTTY | O_NONBLOCK | O_LARGEFILE;

  if (affile_open_file(self->filename->str, flags, -1, -1, -1, 0, 0, 0, 0, fd))
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
affile_sd_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  switch (notify_code)
    {
    case NC_FILE_MOVED:
      { 
        gint fd;
        
        msg_verbose("Follow-mode file source moved, tracking of the new file is started",
                    evt_tag_str("filename", self->filename->str),
                    NULL);
        
        log_pipe_deinit(self->reader, NULL, NULL);
        log_pipe_unref(self->reader);
        
        if (affile_sd_open_file(self, &fd))
          {
            self->reader = log_reader_new(fd_read_new(fd, 0), LR_LOCAL | LR_NOMREAD, s, &self->reader_options);
            log_pipe_append(self->reader, s);
            if (!log_pipe_init(self->reader, NULL, NULL))
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
    }
}

static gboolean
affile_sd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  gint fd;
  gboolean file_opened, open_deferred = FALSE;

  log_reader_options_init(&self->reader_options, cfg);

  file_opened = affile_sd_open_file(self, &fd);
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
      FDRead *fdr;
      
      fdr = fd_read_new(fd, 0);
      fdr->timeout = 10;
      self->reader = log_reader_new(fdr, LR_LOCAL | LR_NOMREAD, s, &self->reader_options);

      if (persist)
        {
          gchar *str;
          off_t cur_pos;
          
          str = persist_config_fetch(persist, affile_sd_format_persist_name(self));
          if (str)
            {
              /* if the file could not be opened, we ignore the last remembered
               * file position, if the file is created in the future we're going
               * to read from the start. */
              
              if (file_opened)
                {
                  cur_pos = strtoll(str, NULL, 10);
                  log_reader_set_pos((LogReader *) self->reader, cur_pos);
                }
              g_free(str);
            }
          
        }
      log_pipe_append(self->reader, s);
      if (!log_pipe_init(self->reader, NULL, NULL))
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
affile_sd_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

  if (persist)
    {
      gchar str[32];
      off_t cur_pos;
      
      cur_pos = log_reader_get_pos((LogReader *) self->reader);
      g_snprintf(str, sizeof(str), "%" G_GINT64_FORMAT, (gint64) cur_pos);
          
      persist_config_add_survivor(persist, affile_sd_format_persist_name(self), str);
    }
  if (self->reader)
    {
      log_pipe_deinit(self->reader, NULL, NULL);
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

  log_drv_free_instance(&self->super);
  g_free(s);
}

LogDriver *
affile_sd_new(gchar *filename, guint32 flags)
{
  AFFileSourceDriver *self = g_new0(AFFileSourceDriver, 1);

  log_drv_init_instance(&self->super);
  self->filename = g_string_new(filename);
  self->flags = flags;
  self->super.super.init = affile_sd_init;
  self->super.super.deinit = affile_sd_deinit;
  self->super.super.notify = affile_sd_notify;
  self->super.super.free_fn = affile_sd_free;
  log_reader_options_defaults(&self->reader_options);
  self->reader_options.follow_filename = self->filename->str;
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
affile_dw_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;
  int fd, flags;
  struct stat st;

  if (cfg)
    self->time_reopen = cfg->time_reopen;

  msg_verbose("Initializing destination file writer",
              evt_tag_str("template", self->owner->filename_template->template->str),
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
    flags = O_WRONLY | O_CREAT | O_APPEND | O_NOCTTY | O_NONBLOCK | O_LARGEFILE;

  self->last_open_stamp = time(NULL);
  if (affile_open_file(self->filename->str, flags, 
                       self->owner->file_uid, self->owner->file_gid, self->owner->file_perm, 
                       self->owner->dir_uid, self->owner->dir_gid, self->owner->dir_perm, 
                       !!(self->owner->flags & AFFILE_CREATE_DIRS), &fd))
    {
      FDWrite *fdw;
      
      self->writer = log_writer_new(LW_FORMAT_FILE, s, &self->owner->writer_options);
        
      if (!log_pipe_init(self->writer, NULL, NULL))
        {
          msg_error("Error initializing log writer", NULL);
          log_pipe_unref(self->writer);
          self->writer = NULL;
          close(fd);
          return FALSE;
        }
      fdw = fd_write_new(fd);
      if (self->owner->flags & AFFILE_FSYNC)
        fdw->fsync = TRUE;
      log_writer_reopen(self->writer, fdw);
      log_pipe_append(&self->super, self->writer);
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
affile_dw_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  if (self->writer)
    {
      log_pipe_deinit(self->writer, NULL, NULL);
      log_pipe_unref(self->writer);
    }
  self->writer = NULL;
  return TRUE;
}

static void
affile_dw_queue(LogPipe *s, LogMessage *lm, gint path_flags)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;
  self->last_msg_stamp = time(NULL);
  if (!s->pipe_next && self->last_open_stamp < self->last_msg_stamp - self->time_reopen)
    {
      log_pipe_init(&self->super, NULL, NULL);
    }
    
  if (s->pipe_next)
    log_pipe_forward_msg(s, lm, path_flags);
  else
    log_msg_drop(lm, path_flags);
}

static void
affile_dw_set_owner(AFFileDestWriter *self, AFFileDestDriver *owner)
{
  if (self->owner)
    log_pipe_unref(&self->owner->super.super);
  log_pipe_ref(&owner->super.super);
  self->owner = owner;
  if (self->writer)
    log_writer_set_options((LogWriter *) self->writer, &owner->writer_options);
  
}

static void
affile_dw_free(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;
  
  g_string_free(self->filename, TRUE);
  log_pipe_unref(&self->owner->super.super);
  
  g_free(s);
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

  g_snprintf(persist_name, sizeof(persist_name), "affile_dd_writers(%s)", self->filename_template->template->str);
  return persist_name;
}

static const gchar *
affile_dd_format_stats_name(AFFileDestDriver *self, gboolean is_pipe)
{
  static gchar stats_name[256];

  g_snprintf(stats_name, sizeof(stats_name),
             "%s(%s)", is_pipe ? "pipe" : "file", self->filename_template->template->str);

  return stats_name;
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
                  evt_tag_str("template", self->filename_template->template->str),
                  evt_tag_str("filename", dw->filename->str),
                  NULL);
      log_pipe_deinit(&dw->super, NULL, NULL);
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
              evt_tag_str("template", self->filename_template->template->str),
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
}


static gboolean
affile_dd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
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
  
  if (self->flags & AFFILE_PIPE)
    log_writer_options_init(&self->writer_options, cfg, LWOF_SHARE_STATS, affile_dd_format_stats_name(self, TRUE));
  else    
    log_writer_options_init(&self->writer_options, cfg, LWOF_NO_STATS, NULL);
    
  self->cfg = cfg;
  
  if ((self->flags & AFFILE_NO_EXPAND) == 0)
    {
      self->reap_timer = g_timeout_add_full(G_PRIORITY_DEFAULT, self->time_reap * 1000 / 2, affile_dd_reap, self, NULL);
      self->writer_hash = persist_config_fetch(persist, affile_dd_format_persist_name(self));
      if (self->writer_hash)
        g_hash_table_foreach(self->writer_hash, affile_dd_reuse_writer, self);
    }
  else
    {
      self->single_writer = persist_config_fetch(persist, affile_dd_format_persist_name(self));
      if (self->single_writer)
        affile_dw_set_owner(self->single_writer, self);
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
  log_pipe_deinit(&writer->super, NULL, NULL);
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

static gboolean
affile_dd_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  /* NOTE: we free all AFFileDestWriter instances here as otherwise we'd
   * have circular references between AFFileDestDriver and file writers */
  if (self->single_writer)
    {
      g_assert(self->writer_hash == NULL);

      persist_config_add(persist, affile_dd_format_persist_name(self), self->single_writer, affile_dd_destroy_writer);
      self->single_writer = NULL;
    }
  else if (self->writer_hash)
    {
      g_assert(self->single_writer == NULL);
      
      persist_config_add(persist, affile_dd_format_persist_name(self), self->writer_hash, affile_dd_destroy_writer_hash);
      self->writer_hash = NULL;
    }
  if (self->reap_timer)
    g_source_remove(self->reap_timer);
  self->cfg = NULL;
  return TRUE;
}

static void
affile_dd_queue(LogPipe *s, LogMessage *msg, gint path_flags)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  AFFileDestWriter *next;

  if (self->flags & AFFILE_NO_EXPAND)
    {
      if (!self->single_writer)
	{
	  next = affile_dw_new(self, g_string_new(self->filename_template->template->str));
	  if (next && log_pipe_init(&next->super, self->cfg, NULL))
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
		    ((self->flags & AFFILE_TMPL_ESCAPE) ? MF_ESCAPE_RESULT : 0) |
		    (self->use_time_recvd ? MF_STAMP_RECVD : 0),
		    TS_FMT_BSD,
		    get_local_timezone_ofs(time(NULL)),
		    0,
		    filename);
      next = g_hash_table_lookup(self->writer_hash, filename->str);
      if (!next)
	{
	  next = affile_dw_new(self, filename);
	  if (!log_pipe_init(&next->super, self->cfg, NULL))
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
    log_pipe_queue(&next->super, msg, path_flags);
  else
    log_msg_drop(msg, path_flags);
}

static void
affile_dd_free(LogPipe *s)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  /* NOTE: this must be NULL as deinit has freed it, otherwise we'd have circular references */
  g_assert(self->single_writer == NULL && self->writer_hash == NULL);

  log_template_unref(self->filename_template);
  log_writer_options_destroy(&self->writer_options);
  log_drv_free_instance(&self->super);
  g_free(self);
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
