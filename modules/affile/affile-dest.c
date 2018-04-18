/*
 * Copyright (c) 2002-2013 Balabit
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
#include "file-opener.h"
#include "affile-dest.h"
#include "driver.h"
#include "messages.h"
#include "serialize.h"
#include "gprocess.h"
#include "stats/stats-registry.h"
#include "mainloop-call.h"
#include "transport/transport-file.h"
#include "logproto-file-writer.h"
#include "transport/transport-file.h"
#include "transport/transport-pipe.h"
#include "logwriter.h"
#include "affile-dest-internal-queue-filter.h"
#include "file-specializations.h"
#include "apphook.h"

#include <iv.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

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

static GList *affile_dest_drivers = NULL;

struct _AFFileDestWriter
{
  LogPipe super;
  GStaticMutex lock;
  AFFileDestDriver *owner;
  gchar *filename;
  LogWriter *writer;
  time_t last_msg_stamp;
  time_t last_open_stamp;
  time_t time_reopen;
  struct iv_timer reap_timer;
  gboolean reopen_pending, queue_pending;
};

static gchar *
affile_dw_format_persist_name(AFFileDestWriter *self)
{
  static gchar persist_name[1024];

  g_snprintf(persist_name, sizeof(persist_name),
             "affile_dw_queue(%s)",
             self->filename);
  return persist_name;
}

static void affile_dd_reap_writer(AFFileDestDriver *self, AFFileDestWriter *dw);

static void
affile_dw_arm_reaper(AFFileDestWriter *self)
{
  /* not yet reaped, set up the next callback */
  iv_validate_now();
  self->reap_timer.expires = iv_now;
  timespec_add_msec(&self->reap_timer.expires, self->owner->time_reap * 1000);
  iv_timer_register(&self->reap_timer);
}

static void
affile_dw_reap(gpointer s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  main_loop_assert_main_thread();

  g_static_mutex_lock(&self->owner->lock);
  if (!log_writer_has_pending_writes((LogWriter *) self->writer) && !self->queue_pending)
    {
      msg_verbose("Destination timed out, reaping",
                  evt_tag_str("template", self->owner->filename_template->template),
                  evt_tag_str("filename", self->filename));
      affile_dd_reap_writer(self->owner, self);
      g_static_mutex_unlock(&self->owner->lock);
    }
  else
    {
      g_static_mutex_unlock(&self->owner->lock);
      affile_dw_arm_reaper(self);
    }
}

static gboolean
affile_dw_reopen(AFFileDestWriter *self)
{
  int fd;
  struct stat st;
  GlobalConfig *cfg;
  LogProtoClient *proto = NULL;

  cfg = log_pipe_get_config(&self->super);
  if (cfg)
    self->time_reopen = cfg->time_reopen;

  msg_verbose("Initializing destination file writer",
              evt_tag_str("template", self->owner->filename_template->template),
              evt_tag_str("filename", self->filename));

  self->last_open_stamp = self->last_msg_stamp;
  if (self->owner->overwrite_if_older > 0 &&
      stat(self->filename, &st) == 0 &&
      st.st_mtime < time(NULL) - self->owner->overwrite_if_older)
    {
      msg_info("Destination file is older than overwrite_if_older(), overwriting",
               evt_tag_str("filename", self->filename),
               evt_tag_int("overwrite_if_older", self->owner->overwrite_if_older));
      unlink(self->filename);
    }

  if (file_opener_open_fd(self->owner->file_opener, self->filename, AFFILE_DIR_WRITE, &fd))
    {
      LogTransport *transport = file_opener_construct_transport(self->owner->file_opener, fd);

      proto = file_opener_construct_dst_proto(self->owner->file_opener, transport,
                                              &self->owner->writer_options.proto_options.super);

      if (!iv_timer_registered(&self->reap_timer))
        main_loop_call((void *(*)(void *)) affile_dw_arm_reaper, self, TRUE);
    }
  else
    {
      msg_error("Error opening file for writing",
                evt_tag_str("filename", self->filename),
                evt_tag_errno(EVT_TAG_OSERROR, errno));
    }

  log_writer_reopen(self->writer, proto);

  return TRUE;
}

static gboolean
affile_dw_init(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!self->writer)
    {
      self->writer = log_writer_new(self->owner->writer_flags, cfg);
    }

  log_writer_set_options(self->writer,
                         s,
                         &self->owner->writer_options,
                         self->owner->super.super.id,
                         self->filename);
  log_writer_set_queue(self->writer, log_dest_driver_acquire_queue(&self->owner->super,
                       affile_dw_format_persist_name(self)));

  if (!log_pipe_init((LogPipe *) self->writer))
    {
      msg_error("Error initializing log writer");
      log_pipe_unref((LogPipe *) self->writer);
      self->writer = NULL;
      return FALSE;
    }
  log_pipe_append(&self->super, (LogPipe *) self->writer);

  return affile_dw_reopen(self);
}

static gboolean
affile_dw_deinit(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  main_loop_assert_main_thread();
  if (self->writer)
    {
      log_pipe_deinit((LogPipe *) self->writer);
    }

  log_writer_set_queue(self->writer, NULL);

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
affile_dw_queue(LogPipe *s, LogMessage *lm, const LogPathOptions *path_options)
{
  if (!affile_dw_queue_enabled_for_msg(lm))
    {
      log_msg_drop(lm, path_options, AT_PROCESSED);
      return;
    }

  AFFileDestWriter *self = (AFFileDestWriter *) s;

  g_static_mutex_lock(&self->lock);
  self->last_msg_stamp = cached_g_current_time_sec();
  if (self->last_open_stamp == 0)
    self->last_open_stamp = self->last_msg_stamp;

  if (!log_writer_opened(self->writer) &&
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
  GlobalConfig *cfg = log_pipe_get_config(&owner->super.super.super);

  if (self->owner)
    log_pipe_unref(&self->owner->super.super.super);
  log_pipe_ref(&owner->super.super.super);
  self->owner = owner;
  self->super.expr_node = owner->super.super.super.expr_node;

  log_pipe_set_config(&self->super, cfg);
  if (self->writer)
    {
      log_pipe_set_config((LogPipe *) self->writer, cfg);
      log_writer_set_options(self->writer,
                             &self->super,
                             &owner->writer_options,
                             self->owner->super.super.id,
                             self->filename);
    }
}

static void
affile_dw_free(LogPipe *s)
{
  AFFileDestWriter *self = (AFFileDestWriter *) s;

  log_pipe_unref((LogPipe *) self->writer);

  g_static_mutex_free(&self->lock);
  self->writer = NULL;
  g_free(self->filename);
  log_pipe_unref(&self->owner->super.super.super);
  log_pipe_free_method(s);
}

static void
affile_dw_notify(LogPipe *s, gint notify_code, gpointer user_data)
{
  switch(notify_code)
    {
    case NC_REOPEN_REQUIRED:
      affile_dw_reopen((AFFileDestWriter *)s);
      break;
    default:
      break;
    }
}

static AFFileDestWriter *
affile_dw_new(const gchar *filename, GlobalConfig *cfg)
{
  AFFileDestWriter *self = g_new0(AFFileDestWriter, 1);

  log_pipe_init_instance(&self->super, cfg);

  self->super.init = affile_dw_init;
  self->super.deinit = affile_dw_deinit;
  self->super.free_fn = affile_dw_free;
  self->super.queue = affile_dw_queue;
  self->super.notify = affile_dw_notify;
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

static void
affile_dw_reopen_writer(gpointer key, gpointer value, gpointer user_data)
{
  AFFileDestWriter *writer = (AFFileDestWriter *) value;
  affile_dw_reopen(writer);
}

static void
affile_dd_reopen_all_writers(gpointer data, gpointer user_data)
{
  AFFileDestDriver *driver = (AFFileDestDriver *) data;
  if (driver->single_writer)
    affile_dw_reopen(driver->single_writer);
  else if (driver->writer_hash)
    g_hash_table_foreach(driver->writer_hash, affile_dw_reopen_writer, NULL);
}

static void
affile_dd_register_reopen_hook(gint hook_type, gpointer user_data)
{
  g_list_foreach(affile_dest_drivers, affile_dd_reopen_all_writers, NULL);
}

void
affile_dd_set_create_dirs(LogDriver *s, gboolean create_dirs)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  self->file_opener_options.create_dirs = create_dirs;
}

void
affile_dd_set_overwrite_if_older(LogDriver *s, gint overwrite_if_older)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  self->overwrite_if_older = overwrite_if_older;
}

void
affile_dd_set_fsync(LogDriver *s, gboolean use_fsync)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  self->use_fsync = use_fsync;
}

static inline const gchar *
affile_dd_format_persist_name(const LogPipe *s)
{
  const AFFileDestDriver *self = (const AFFileDestDriver *)s;
  static gchar persist_name[1024];

  if (s->persist_name)
    g_snprintf(persist_name, sizeof(persist_name), "affile_dd.%s.writers", s->persist_name);
  else
    g_snprintf(persist_name, sizeof(persist_name), "affile_dd_writers(%s)",
               self->filename_template->template);

  return persist_name;
}

/* DestDriver lock must be held before calling this function */
static void
affile_dd_reap_writer(AFFileDestDriver *self, AFFileDestWriter *dw)
{
  LogWriter *writer = (LogWriter *)dw->writer;

  main_loop_assert_main_thread();

  if (self->filename_is_a_template)
    {
      /* remove from hash table */
      g_hash_table_remove(self->writer_hash, dw->filename);
    }
  else
    {
      g_assert(dw == self->single_writer);
      self->single_writer = NULL;
    }

  LogQueue *queue = log_writer_get_queue(writer);
  log_pipe_deinit(&dw->super);
  log_dest_driver_release_queue(&self->super, queue);
  log_pipe_unref(&dw->super);
}


/**
 * affile_dd_reuse_writer:
 *
 * This function is called as a g_hash_table_foreach() callback to set the
 * owner of each writer, previously connected to an AFileDestDriver instance
 * in an earlier configuration. This way AFFileDestWriter instances are
 * remembered across reloads.
 *
 **/
static void
affile_dd_reuse_writer(gpointer key, gpointer value, gpointer user_data)
{
  AFFileDestDriver *self = (AFFileDestDriver *) user_data;
  AFFileDestWriter *writer = (AFFileDestWriter *) value;

  affile_dw_set_owner(writer, self);
  if (!log_pipe_init(&writer->super))
    {
      affile_dw_set_owner(writer, NULL);
      log_pipe_unref(&writer->super);
      g_hash_table_remove(self->writer_hash, key);
    }
}


static gboolean
affile_dd_init(LogPipe *s)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_dest_driver_init_method(s))
    return FALSE;

  if (self->file_opener_options.create_dirs == -1)
    self->file_opener_options.create_dirs = cfg->create_dirs;
  if (self->time_reap == -1)
    self->time_reap = cfg->time_reap;

  file_opener_options_init(&self->file_opener_options, cfg);
  file_opener_set_options(self->file_opener, &self->file_opener_options);
  log_writer_options_init(&self->writer_options, cfg, 0);

  if (self->filename_is_a_template)
    {
      self->writer_hash = cfg_persist_config_fetch(cfg, affile_dd_format_persist_name(s));
      if (self->writer_hash)
        g_hash_table_foreach(self->writer_hash, affile_dd_reuse_writer, self);
    }
  else
    {
      self->single_writer = cfg_persist_config_fetch(cfg, affile_dd_format_persist_name(s));
      if (self->single_writer)
        {
          affile_dw_set_owner(self->single_writer, self);
          if (!log_pipe_init(&self->single_writer->super))
            {
              log_pipe_unref(&self->single_writer->super);
              return FALSE;
            }
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
      cfg_persist_config_add(cfg, affile_dd_format_persist_name(s), self->single_writer,
                             affile_dd_destroy_writer, FALSE);
      self->single_writer = NULL;
    }
  else if (self->writer_hash)
    {
      g_assert(self->single_writer == NULL);

      g_hash_table_foreach(self->writer_hash, affile_dd_deinit_writer, NULL);
      cfg_persist_config_add(cfg, affile_dd_format_persist_name(s), self->writer_hash,
                             affile_dd_destroy_writer_hash, FALSE);
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
  AFFileDestWriter *next;

  main_loop_assert_main_thread();
  if (!self->filename_is_a_template)
    {
      if (!self->single_writer)
        {
          next = affile_dw_new(self->filename_template->template, log_pipe_get_config(&self->super.super.super));
          affile_dw_set_owner(next, self);
          if (next && log_pipe_init(&next->super))
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
          next = affile_dw_new(filename->str, log_pipe_get_config(&self->super.super.super));
          affile_dw_set_owner(next, self);
          if (!log_pipe_init(&next->super))
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
affile_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;
  AFFileDestWriter *next;
  gpointer args[2] = { self, NULL };

  if (!self->filename_is_a_template)
    {

      /* we need to lock single_writer in order to get a reference and
       * make sure it is not a stale pointer by the time we ref it */

      g_static_mutex_lock(&self->lock);
      if (!self->single_writer)
        {
          g_static_mutex_unlock(&self->lock);
          next = main_loop_call((void *(*)(void *)) affile_dd_open_writer, args, TRUE);
        }
      else
        {
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
      log_template_format(self->filename_template, msg, &self->writer_options.template_options, LTZ_LOCAL, 0, NULL, filename);

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
      log_msg_add_ack(msg, path_options);
      log_pipe_queue(&next->super, log_msg_ref(msg), path_options);
      next->queue_pending = FALSE;
      log_pipe_unref(&next->super);
    }

  log_dest_driver_queue_method(s, msg, path_options);
}

static void
affile_dd_free(LogPipe *s)
{
  AFFileDestDriver *self = (AFFileDestDriver *) s;

  g_static_mutex_free(&self->lock);
  affile_dest_drivers = g_list_remove(affile_dest_drivers, self);

  /* NOTE: this must be NULL as deinit has freed it, otherwise we'd have circular references */
  g_assert(self->single_writer == NULL && self->writer_hash == NULL);

  log_template_unref(self->filename_template);
  log_writer_options_destroy(&self->writer_options);
  file_opener_options_deinit(&self->file_opener_options);
  file_opener_free(self->file_opener);
  log_dest_driver_free(s);
}

AFFileDestDriver *
affile_dd_new_instance(gchar *filename, GlobalConfig *cfg)
{
  AFFileDestDriver *self = g_new0(AFFileDestDriver, 1);

  log_dest_driver_init_instance(&self->super, cfg);
  self->super.super.super.init = affile_dd_init;
  self->super.super.super.deinit = affile_dd_deinit;
  self->super.super.super.queue = affile_dd_queue;
  self->super.super.super.free_fn = affile_dd_free;
  self->super.super.super.generate_persist_name = affile_dd_format_persist_name;
  self->filename_template = log_template_new(cfg, NULL);
  log_template_compile(self->filename_template, filename, NULL);
  log_writer_options_defaults(&self->writer_options);
  self->writer_options.mark_mode = MM_NONE;
  self->writer_options.stats_level = STATS_LEVEL1;
  self->writer_flags = LW_FORMAT_FILE;

  if (strchr(filename, '$') != NULL)
    {
      self->filename_is_a_template = TRUE;
    }
  file_opener_options_defaults(&self->file_opener_options);

  self->time_reap = -1;
  g_static_mutex_init(&self->lock);

  affile_dest_drivers = g_list_append(affile_dest_drivers, self);

  return self;
}

LogDriver *
affile_dd_new(gchar *filename, GlobalConfig *cfg)
{
  AFFileDestDriver *self = affile_dd_new_instance(filename, cfg);

  self->writer_flags |= LW_SOFT_FLOW_CONTROL;
  self->writer_options.stats_source = SCS_FILE;
  self->file_opener = file_opener_for_regular_dest_files_new(&self->writer_options, &self->use_fsync);
  return &self->super.super;
}

void
affile_dd_global_init(void)
{
  register_application_hook(AH_REOPEN, affile_dd_register_reopen_hook, NULL);
}
