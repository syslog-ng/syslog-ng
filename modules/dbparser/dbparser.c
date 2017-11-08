/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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

#include "dbparser.h"
#include "patterndb.h"
#include "radix.h"
#include "apphook.h"
#include "reloc.h"
#include "stateful-parser.h"

#include <sys/stat.h>
#include <iv.h>
#include <string.h>


struct _LogDBParser
{
  StatefulParser super;
  GStaticMutex lock;
  struct iv_timer tick;
  PatternDB *db;
  gchar *db_file;
  time_t db_file_last_check;
  ino_t db_file_inode;
  time_t db_file_mtime;
  gboolean db_file_reloading;
  gboolean drop_unmatched;
};

static void
log_db_parser_emit(LogMessage *msg, gboolean synthetic, gpointer user_data)
{
  LogDBParser *self = (LogDBParser *) user_data;

  if (synthetic)
    {
      stateful_parser_emit_synthetic(&self->super, msg);
      msg_debug("db-parser: emitting synthetic message",
                evt_tag_str("msg", log_msg_get_value(msg, LM_V_MESSAGE, NULL)));
    }
}

static void
log_db_parser_reload_database(LogDBParser *self)
{
  struct stat st;
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super.super);

  if (stat(self->db_file, &st) < 0)
    {
      msg_error("Error stating pattern database file, no automatic reload will be performed",
                evt_tag_str("error", g_strerror(errno)));
      return;
    }
  if ((self->db_file_inode == st.st_ino && self->db_file_mtime == st.st_mtime))
    {
      return;
    }

  self->db_file_inode = st.st_ino;
  self->db_file_mtime = st.st_mtime;

  if (!pattern_db_reload_ruleset(self->db, cfg, self->db_file))
    {
      msg_error("Error reloading pattern database, no automatic reload will be performed");
    }
  else
    {
      /* free the old database if the new was loaded successfully */
      msg_notice("Log pattern database reloaded",
                 evt_tag_str("file", self->db_file),
                 evt_tag_str("version", pattern_db_get_ruleset_version(self->db)),
                 evt_tag_str("pub_date", pattern_db_get_ruleset_pub_date(self->db)));
    }

}

static void
log_db_parser_timer_tick(gpointer s)
{
  LogDBParser *self = (LogDBParser *) s;

  pattern_db_timer_tick(self->db);
  iv_validate_now();
  self->tick.expires = iv_now;
  self->tick.expires.tv_sec++;
  iv_timer_register(&self->tick);
}

static gchar *
log_db_parser_format_persist_name(LogDBParser *self)
{
  static gchar persist_name[512];

  g_snprintf(persist_name, sizeof(persist_name), "db-parser(%s)", self->db_file);
  return persist_name;
}

static gboolean
log_db_parser_init(LogPipe *s)
{
  LogDBParser *self = (LogDBParser *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  self->db = cfg_persist_config_fetch(cfg, log_db_parser_format_persist_name(self));
  if (self->db)
    {
      struct stat st;

      if (stat(self->db_file, &st) < 0)
        {
          msg_error("Error stating pattern database file, no automatic reload will be performed",
                    evt_tag_str("error", g_strerror(errno)));
        }
      else if (self->db_file_inode != st.st_ino || self->db_file_mtime != st.st_mtime)
        {
          log_db_parser_reload_database(self);
          self->db_file_inode = st.st_ino;
          self->db_file_mtime = st.st_mtime;
        }
    }
  else
    {
      self->db = pattern_db_new();
      log_db_parser_reload_database(self);
    }
  if (self->db)
    pattern_db_set_emit_func(self->db, log_db_parser_emit, self);
  iv_validate_now();
  IV_TIMER_INIT(&self->tick);
  self->tick.cookie = self;
  self->tick.handler = log_db_parser_timer_tick;
  self->tick.expires = iv_now;
  self->tick.expires.tv_sec++;
  self->tick.expires.tv_nsec = 0;
  iv_timer_register(&self->tick);
  if (!self->db)
    return FALSE;
  return stateful_parser_init_method(s);
}

static gboolean
log_db_parser_deinit(LogPipe *s)
{
  LogDBParser *self = (LogDBParser *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (iv_timer_registered(&self->tick))
    {
      iv_timer_unregister(&self->tick);
    }

  cfg_persist_config_add(cfg, log_db_parser_format_persist_name(self), self->db, (GDestroyNotify) pattern_db_free, FALSE);
  self->db = NULL;
  return stateful_parser_deinit_method(s);
}

static gboolean
log_db_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const char *input,
                      gsize input_len)
{
  LogDBParser *self = (LogDBParser *) s;
  gboolean matched = FALSE;

  if (G_UNLIKELY(!self->db_file_reloading && (self->db_file_last_check == 0
                                              || self->db_file_last_check < (*pmsg)->timestamps[LM_TS_RECVD].tv_sec - 5)))
    {
      /* first check if we need to reload without doing a lock, then grab
       * the lock, recheck the condition to rule out parallel database
       * reloads. This avoids a lock in the fast path. */

      g_static_mutex_lock(&self->lock);

      if (!self->db_file_reloading && (self->db_file_last_check == 0
                                       || self->db_file_last_check < (*pmsg)->timestamps[LM_TS_RECVD].tv_sec - 5))
        {
          self->db_file_last_check = (*pmsg)->timestamps[LM_TS_RECVD].tv_sec;
          self->db_file_reloading = TRUE;
          g_static_mutex_unlock(&self->lock);

          /* only one thread may come here, the others may continue to use self->db, until we update it here. */
          log_db_parser_reload_database(self);

          g_static_mutex_lock(&self->lock);
          self->db_file_reloading = FALSE;
        }
      g_static_mutex_unlock(&self->lock);
    }
  if (self->db)
    {
      log_msg_make_writable(pmsg, path_options);

      if (G_UNLIKELY(self->super.super.template))
        matched = pattern_db_process_with_custom_message(self->db, *pmsg, input, input_len);
      else
        matched = pattern_db_process(self->db, *pmsg);
    }

  if (!self->drop_unmatched)
    matched = TRUE;
  return matched;
}

void
log_db_parser_set_db_file(LogDBParser *self, const gchar *db_file)
{
  if (self->db_file)
    g_free(self->db_file);
  self->db_file = g_strdup(db_file);
}

void
log_db_parser_set_drop_unmatched(LogDBParser *self, gboolean setting)
{
  self->drop_unmatched = setting;
}

/*
 * NOTE: we could be smarter than this by sharing the radix tree in this case.
 */
static LogPipe *
log_db_parser_clone(LogPipe *s)
{
  LogDBParser *cloned;
  LogDBParser *self = (LogDBParser *) s;

  cloned = (LogDBParser *) log_db_parser_new(s->cfg);
  log_db_parser_set_db_file(cloned, self->db_file);
  return &cloned->super.super.super;
}

static void
log_db_parser_free(LogPipe *s)
{
  LogDBParser *self = (LogDBParser *) s;

  g_static_mutex_free(&self->lock);

  if (self->db)
    pattern_db_free(self->db);

  if (self->db_file)
    g_free(self->db_file);
  stateful_parser_free_method(s);
}

LogParser *
log_db_parser_new(GlobalConfig *cfg)
{
  LogDBParser *self = g_new0(LogDBParser, 1);

  stateful_parser_init_instance(&self->super, cfg);
  self->super.super.super.free_fn = log_db_parser_free;
  self->super.super.super.init = log_db_parser_init;
  self->super.super.super.deinit = log_db_parser_deinit;
  self->super.super.super.clone = log_db_parser_clone;
  self->super.super.process = log_db_parser_process;
  self->db_file = g_strdup(get_installation_path_for(PATH_PATTERNDB_FILE));
  g_static_mutex_init(&self->lock);
  if (cfg_is_config_version_older(cfg, 0x0303))
    {
      msg_warning_once("WARNING: The default behaviour for injecting messages in db-parser() has changed in " VERSION_3_3
                       " from internal to pass-through, use an explicit inject-mode(internal) option for old behaviour");
      self->super.inject_mode = LDBP_IM_INTERNAL;
    }
  return &self->super.super;
}
