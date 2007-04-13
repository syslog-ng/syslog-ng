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
  if (uid != -1 || gid != -1)
    chown(name, uid, gid);
  if (mode != -1)
    chmod(name, mode);
  return *fd != -1;
}

static gboolean
affile_sd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;
  int fd, flags;

  if (self->flags & AFFILE_PIPE)
    flags = O_RDWR | O_NOCTTY | O_NONBLOCK;
  else
    {
      flags = O_RDONLY | O_NOCTTY | O_NONBLOCK;
    }

  log_reader_options_init(&self->reader_options, cfg);

  if (affile_open_file(self->filename->str, flags, -1, -1, -1, 0, 0, 0, 0, &fd))
    {
      self->reader = log_reader_new(fd_read_new(fd, 0), LR_LOCAL, s, &self->reader_options);
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
      return FALSE;
    }
  return TRUE;
}

static gboolean
affile_sd_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFFileSourceDriver *self = (AFFileSourceDriver *) s;

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
  self->super.super.free_fn = affile_sd_free;
  log_reader_options_defaults(&self->reader_options);
  return &self->super;
}


typedef struct _AFFileDestWriter
{
  LogPipe super;
  AFFileDestDriver *owner;
  GString *filename;
  LogPipe *writer;
  time_t last_msg_stamp;
} AFFileDestWriter;

static gboolean
affile_dw_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;
  int fd, flags;

  msg_verbose("Initializing destination file writer",
              evt_tag_str("template", self->owner->filename_template->template->str),
              evt_tag_str("filename", self->filename->str),
              NULL);

  if (self->owner->flags & AFFILE_PIPE)
    flags = O_RDWR | O_NOCTTY | O_NONBLOCK;
  else
    flags = O_WRONLY | O_CREAT | O_APPEND | O_NOCTTY | O_NONBLOCK;

  if (affile_open_file(self->filename->str, flags, 
                       self->owner->file_uid, self->owner->file_gid, self->owner->file_perm, 
                       self->owner->dir_uid, self->owner->dir_gid, self->owner->dir_perm, 
                       !!(self->owner->flags & AFFILE_CREATE_DIRS), &fd))
    {
      self->writer = log_writer_new(LW_FORMAT_FILE, s, &self->owner->writer_options);
        
      if (!log_pipe_init(self->writer, NULL, NULL))
        {
          msg_error("Error initializing log writer", NULL);
          log_pipe_unref(self->writer);
          self->writer = NULL;
          close(fd);
          return FALSE;
        }
      log_writer_reopen(self->writer, fd_write_new(fd));
      log_pipe_append(&self->super, self->writer);
    }
  else
    {
      msg_error("Error opening file for writing",
                evt_tag_str("filename", self->filename->str),
                evt_tag_errno(EVT_TAG_OSERROR, errno),
                NULL);
      return FALSE;
    }
  return TRUE;
}

static gboolean
affile_dw_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;
  log_pipe_deinit(self->writer, NULL, NULL);
  log_pipe_unref(self->writer);
  self->writer = NULL;
  return TRUE;
}

static void
affile_dw_queue(LogPipe *s, LogMessage *lm, gint path_flags)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  self->last_msg_stamp = time(NULL);
  log_pipe_forward_msg(s, lm, path_flags);
}

static void
affile_dw_free(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;
  
  g_string_free(self->filename, TRUE);
  log_pipe_unref(&self->owner->super.super);
  
  g_free(s);
}

static LogPipe *
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
  
  /* we have to take care about freeing filename later. 
     This avoids a move of the filename. */
  self->filename = filename;
  return &self->super;
}

void 
affile_dd_set_sync_freq(LogPipe *s, gint sync_freq)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->sync_freq = sync_freq;
}

void 
affile_dd_set_compress(LogPipe *s, gboolean compress G_GNUC_UNUSED)
{
  msg_error("On-the-fly file compression is not supported", NULL);
}

void 
affile_dd_set_encrypt()
{
  msg_error("On-the-fly file encryption is not supported", NULL);
}

void 
affile_dd_set_file_uid(LogPipe *s, uid_t file_uid)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->file_uid = file_uid;
}

void 
affile_dd_set_file_gid(LogPipe *s, gid_t file_gid)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->file_gid = file_gid;
}

void 
affile_dd_set_file_perm(LogPipe *s, mode_t file_perm)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->file_perm = file_perm;
}

void 
affile_dd_set_dir_uid(LogPipe *s, uid_t dir_uid)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->dir_uid = dir_uid;
}

void 
affile_dd_set_dir_gid(LogPipe *s, gid_t dir_gid)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->dir_gid = dir_gid;
}

void 
affile_dd_set_dir_perm(LogPipe *s, mode_t dir_perm)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  self->dir_perm = dir_perm;
}

void 
affile_dd_set_create_dirs(LogPipe *s, gboolean create_dirs)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  if (create_dirs)
    self->flags |= AFFILE_CREATE_DIRS;
  else 
    self->flags &= ~AFFILE_CREATE_DIRS;
}

void 
affile_dd_set_file_template(LogDriver *s, gchar *template)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  g_string_assign(self->template, template);
}

void 
affile_dd_set_template_escape(LogDriver *s, gboolean enable)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  if (self->writer_options.template && self->writer_options.template->def_inline)
    {
      log_template_set_escape(self->writer_options.template, enable);
    }
  else
    {
      msg_notice("Macro escaping can only be specified for inline templates", NULL);
    }
}

void 
affile_dd_set_fsync()
{
}

static time_t reap_now = 0;

static gboolean
affile_dd_reap_writers(gpointer key, gpointer value, gpointer user_data)
{
  AFFileDestDriver *self = (AFFileDestDriver *) user_data;
  AFFileDestWriter *dw = (AFFileDestWriter *) value;
  
  if ((reap_now - dw->last_msg_stamp) >= self->time_reap)
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

static gboolean
affile_dd_init(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  if (self->sync_freq == -1)
    self->sync_freq = cfg->sync_freq;

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
  log_writer_options_init(&self->writer_options, cfg, FALSE);
  self->cfg = cfg;
  
  if ((self->flags & AFFILE_NO_EXPAND) == 0)
    {
      self->reap_timer = g_timeout_add_full(G_PRIORITY_LOW, self->time_reap * 1000 / 2, affile_dd_reap, self, NULL);
    }
  return TRUE;
}

static gboolean
affile_dd_remove_writers(gpointer key, gpointer value, gpointer user_data)
{
  AFFileDestDriver *self = (AFFileDestDriver *) user_data;
  LogPipe *writer = (LogPipe *) value;
  
  log_pipe_deinit(writer, self->cfg, NULL);
  log_pipe_unref(writer);
  return TRUE;
}

static gboolean
affile_dd_deinit(LogPipe *s, GlobalConfig *cfg, PersistentConfig *persist)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  if (self->writer)
    log_pipe_deinit(self->writer, cfg, NULL);
  if (self->writer_hash)
    g_hash_table_foreach_remove(self->writer_hash, affile_dd_remove_writers, self);
  if (self->reap_timer)
    g_source_remove(self->reap_timer);
  self->cfg = NULL;
  return TRUE;
}

static void
affile_dd_queue(LogPipe *s, LogMessage *msg, gint path_flags)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  LogPipe *next;

  if (self->flags & AFFILE_NO_EXPAND)
    {
      if (!self->writer)
	{
	  next = affile_dw_new(self, g_string_new(self->filename_template->template->str));
	  if (next && log_pipe_init(next, self->cfg, NULL))
	    {
	      self->writer = next;
	    }
	  else
	    {
	      log_pipe_unref(next);
	      next = NULL;
	    }
	}
      else
        {
          next = self->writer;
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
		    ((self->flags & AFFILE_STAMP_RECVD) ? MF_STAMP_RECVD : 0),
		    TS_FMT_BSD,
		    get_local_timezone_ofs(),
		    filename);
      next = g_hash_table_lookup(self->writer_hash, filename->str);
      if (!next)
	{
	  next = affile_dw_new(self, filename);
	  if (!log_pipe_init(next, self->cfg, NULL))
	    {
	      log_pipe_unref(next);
	      next = NULL;
	    }
	  else
	    g_hash_table_insert(self->writer_hash, filename->str, next);
	}
      else
        g_string_free(filename, TRUE);
    }
  if (next)
    log_pipe_queue(next, msg, path_flags);
  else
    log_msg_drop(msg, path_flags);
}

static void
affile_dd_free(LogPipe *s)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  
  log_template_unref(self->filename_template);
  g_string_free(self->template, TRUE);
  log_pipe_unref(self->writer);
  if (self->writer_hash)
    g_hash_table_destroy(self->writer_hash);

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
  self->template = g_string_sized_new(0);
  self->flags = flags;
  self->file_uid = self->file_gid = -1;
  self->file_perm = -1;
  self->dir_uid = self->dir_uid = -1;
  self->dir_perm = -1;
  log_writer_options_defaults(&self->writer_options);
  if (strchr(filename, '$') == NULL)
    {
      self->flags |= AFFILE_NO_EXPAND;
    }
  self->time_reap = -1;
  return &self->super;
}
