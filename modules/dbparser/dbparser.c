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

#include "dbparser.h"
#include "patterndb.h"
#include "radix.h"
#include "apphook.h"

#include <sys/stat.h>

struct _LogDBParser
{
  LogParser super;
  PatternDB *db;
  GlobalConfig *cfg;
  gchar *db_file;
  time_t db_file_last_check;
  ino_t db_file_inode;
  time_t db_file_mtime;
};

static void
log_db_parser_reload_database(LogDBParser *self)
{
  struct stat st;
  PatternDB *db_new;

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

  db_new = pattern_db_new();
  if (!pattern_db_load(db_new, self->cfg, self->db_file, NULL))
    {
      msg_error("Error reloading pattern database, no automatic reload will be performed", NULL);
      /* restore the old object pointers */
      pattern_db_free(db_new);
    }
  else
    {
      /* free the old database if the new was loaded successfully */
      if (self->db)
        pattern_db_free(self->db);
      self->db = db_new;

      msg_notice("Log pattern database reloaded",
                 evt_tag_str("file", self->db_file),
                 evt_tag_str("version", self->db->version),
                 evt_tag_str("pub_date", self->db->pub_date),
                 NULL);

    }

}

static gboolean
log_db_parser_process(LogParser *s, LogMessage *msg, const char *input)
{
  LogDBParser *self = (LogDBParser *) s;

  if (G_UNLIKELY(self->db_file_last_check == 0 || self->db_file_last_check < msg->timestamps[LM_TS_RECVD].time.tv_sec - 5))
    {
      self->db_file_last_check = msg->timestamps[LM_TS_RECVD].time.tv_sec;
      log_db_parser_reload_database(self);
    }

  pattern_db_lookup(self->db, msg, NULL);
  return TRUE;
}

void
log_db_parser_set_db_file(LogDBParser *self, const gchar *db_file)
{
  if (self->db_file)
    g_free(self->db_file);
  self->db_file = g_strdup(db_file);
}

static void
log_db_parser_post_config_hook(gint type, gpointer user_data)
{
  LogDBParser *self = (LogDBParser *) user_data;

  log_db_parser_reload_database(self);
}

static void
log_db_parser_free(LogParser *s)
{
  LogDBParser *self = (LogDBParser *) s;

  if (self->db)
    pattern_db_free(self->db);

  if (self->db_file)
    g_free(self->db_file);
}

LogParser *
log_db_parser_new(void)
{
  LogDBParser *self = g_new0(LogDBParser, 1);

  self->super.process = log_db_parser_process;
  self->super.free_fn = log_db_parser_free;
  self->db_file = g_strdup(PATH_PATTERNDB_FILE);
  self->cfg = configuration;

  register_application_hook(AH_POST_CONFIG_LOADED, log_db_parser_post_config_hook, self);
  return &self->super;
}
