/*
 * Copyright (c) 2002-2013 BalaBit IT Ltd, Budapest, Hungary
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

#include <sys/stat.h>
#include <iv.h>
#include <string.h>

typedef enum
{
  LDBP_IM_PASSTHROUGH = 0,
  LDBP_IM_INTERNAL = 1,
} LogDBParserInjectMode;

struct _LogDBParser
{
  LogParser super;
  GStaticMutex lock;
  struct iv_timer tick;
  PatternDB *db;
  gchar *db_file;
  time_t db_file_last_check;
  ino_t db_file_inode;
  time_t db_file_mtime;
  gboolean db_file_reloading;
  LogDBParserInjectMode inject_mode;
};

static void
log_db_parser_emit(LogMessage *msg, gboolean synthetic, gpointer user_data)
{
  LogDBParser *self = (LogDBParser *) user_data;

  if (synthetic)
    {
      if (self->inject_mode == LDBP_IM_PASSTHROUGH)
        {
          LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

          path_options.ack_needed = FALSE;
          log_pipe_forward_msg(&self->super.super, log_msg_ref(msg), &path_options);
        }
      else
        {
          msg_post_message(log_msg_ref(msg));
        }
      msg_debug("db-parser: emitting synthetic message",
                evt_tag_str("msg", log_msg_get_value(msg, LM_V_MESSAGE, NULL)),
                NULL);
    }
}

static void
log_db_parser_reload_database(LogDBParser *self)
{
  struct stat st;
  GlobalConfig *cfg = log_pipe_get_config(&self->super.super);

  if (stat(self->db_file, &st) < 0)
    {
      msg_error("Error stating pattern database file, no automatic reload will be performed",
                evt_tag_str("error", g_strerror(errno)),
                NULL);
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
      msg_error("Error reloading pattern database, no automatic reload will be performed", NULL);
    }
  else
    {
      /* free the old database if the new was loaded successfully */
      msg_notice("Log pattern database reloaded",
                 evt_tag_str("file", self->db_file),
                 evt_tag_str("version", pattern_db_get_ruleset_version(self->db)),
                 evt_tag_str("pub_date", pattern_db_get_ruleset_pub_date(self->db)),
                 NULL);
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
                    evt_tag_str("error", g_strerror(errno)),
                    NULL);
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
  return self->db != NULL;
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
  return TRUE;
}

static gboolean
log_db_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const char *input, gsize input_len)
{
  LogDBParser *self = (LogDBParser *) s;

  if (G_UNLIKELY(!self->db_file_reloading && (self->db_file_last_check == 0 || self->db_file_last_check < (*pmsg)->timestamps[LM_TS_RECVD].tv_sec - 5)))
    {
      /* first check if we need to reload without doing a lock, then grab
       * the lock, recheck the condition to rule out parallel database
       * reloads. This avoids a lock in the fast path. */

      g_static_mutex_lock(&self->lock);

      if (!self->db_file_reloading && (self->db_file_last_check == 0 || self->db_file_last_check < (*pmsg)->timestamps[LM_TS_RECVD].tv_sec - 5))
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
      PDBInput pdb_input;

      log_msg_make_writable(pmsg, path_options);

      pdb_input.msg = *pmsg;
      pdb_input.program_handle = LM_V_PROGRAM;
      pdb_input.message_handle = LM_V_MESSAGE;
      pdb_input.message_len = 0;

      if (self->super.template)
        {
          /* we are using a user-supplied template() in place of $MESSAGE */
          pdb_input.message_handle = LM_V_NONE;
          pdb_input.message_string = input;
          pdb_input.message_len = input_len;
        }
      pattern_db_process(self->db, &pdb_input);
    }
  return TRUE;
}

void
log_db_parser_set_db_file(LogDBParser *self, const gchar *db_file)
{
  if (self->db_file)
    g_free(self->db_file);
  self->db_file = g_strdup(db_file);
}

void
log_db_parser_set_inject_mode(LogDBParser *self, const gchar *inject_mode)
{
  if (strcmp(inject_mode, "internal") == 0)
    {
      self->inject_mode = LDBP_IM_INTERNAL;
    }
  else if (strcmp(inject_mode, "pass-through") == 0 || strcmp(inject_mode, "pass_through") == 0)
    {
      self->inject_mode = LDBP_IM_PASSTHROUGH;
    }
  else
    {
      msg_warning("Unknown inject-mode specified for db-parser",
                  evt_tag_str("inject-mode", inject_mode),
                  NULL);
    }
}

/*
 * NOTE: we could be smarter than this by sharing the radix tree in this case.
 */
static LogPipe *
log_db_parser_clone(LogPipe *s)
{
  LogDBParser *clone;
  LogDBParser *self = (LogDBParser *) s;

  clone = (LogDBParser *) log_db_parser_new(s->cfg);
  log_db_parser_set_db_file(clone, self->db_file);
  return &clone->super.super;
}

static void
log_db_parser_free(LogPipe *s)
{
  LogDBParser *self = (LogDBParser *) s;

  if (self->db)
    pattern_db_free(self->db);

  if (self->db_file)
    g_free(self->db_file);
  log_parser_free_method(s);
}

LogParser *
log_db_parser_new(GlobalConfig *cfg)
{
  LogDBParser *self = g_new0(LogDBParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.free_fn = log_db_parser_free;
  self->super.super.init = log_db_parser_init;
  self->super.super.deinit = log_db_parser_deinit;
  self->super.super.clone = log_db_parser_clone;
  self->super.process = log_db_parser_process;
  self->db_file = g_strdup(get_installation_path_for(PATH_PATTERNDB_FILE));
  g_static_mutex_init(&self->lock);
  if (cfg_is_config_version_older(cfg, 0x0303))
    {
      msg_warning("WARNING: The default behaviour for injecting messages in db-parser() has changed in " VERSION_3_3 " from internal to pass-through, use an explicit inject-mode(internal) option for old behaviour", NULL);
      self->inject_mode = LDBP_IM_INTERNAL;
    }
  else
    self->inject_mode = LDBP_IM_PASSTHROUGH;
  return &self->super;
}
