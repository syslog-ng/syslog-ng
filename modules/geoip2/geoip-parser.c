/*
 * Copyright (c) 2017 Balabit
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

#include "geoip-parser.h"
#include "maxminddb-helper.h"

typedef struct _GeoIPParser GeoIPParser;

struct _GeoIPParser
{
  LogParser super;
  MMDB_s *database;

  gchar *database_path;
  gchar *prefix;
};

void
geoip_parser_set_prefix(LogParser *s, const gchar *prefix)
{
  GeoIPParser *self = (GeoIPParser *) s;

  g_free(self->prefix);
  self->prefix = g_strdup(prefix);
}

void
geoip_parser_set_database_path(LogParser *s, const gchar *database_path)
{
  GeoIPParser *self = (GeoIPParser *) s;

  g_free(self->database_path);
  self->database_path = g_strdup(database_path);
}

static gboolean
_mmdb_load_entry_data_list(GeoIPParser *self, const gchar *input, MMDB_entry_data_list_s **entry_data_list)
{
  int _gai_error, mmdb_error;
  MMDB_lookup_result_s result =
    MMDB_lookup_string(self->database, input, &_gai_error, &mmdb_error);

  if (!result.found_entry)
    {
      if (_gai_error != 0)
        msg_error("geoip2(): getaddrinfo failed",
                  evt_tag_str("gai_error", gai_strerror(_gai_error)),
                  evt_tag_str("ip", input),
                  log_pipe_location_tag(&self->super.super));

      if (mmdb_error != MMDB_SUCCESS )
        msg_error("geoip2(): maxminddb error",
                  evt_tag_str("error", MMDB_strerror(mmdb_error)),
                  evt_tag_str("ip", input),
                  log_pipe_location_tag(&self->super.super));

      return FALSE;
    }

  mmdb_error = MMDB_get_entry_data_list(&result.entry, entry_data_list);
  if (MMDB_SUCCESS != mmdb_error)
    {
      msg_debug("GeoIP2: MMDB_get_entry_data_list",
                evt_tag_str("error", MMDB_strerror(mmdb_error)));
      return FALSE;
    }
  return TRUE;
}

static gboolean
maxminddb_parser_process(LogParser *s, LogMessage **pmsg,
                         const LogPathOptions *path_options,
                         const gchar *input, gsize input_len)
{
  GeoIPParser *self = (GeoIPParser *) s;
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);
  msg_trace("geoip2-parser message processing started",
            evt_tag_str ("input", input),
            evt_tag_str ("prefix", self->prefix),
            evt_tag_printf("msg", "%p", *pmsg));

  MMDB_entry_data_list_s *entry_data_list;
  if (!_mmdb_load_entry_data_list(self, input, &entry_data_list))
    return TRUE;

  GArray *path = g_array_new(TRUE, FALSE, sizeof(gchar *));
  g_array_append_val(path, self->prefix);

  gint status;
  dump_geodata_into_msg(msg, entry_data_list, path, &status);

  MMDB_free_entry_data_list(entry_data_list);
  g_array_free(path, TRUE);

  return TRUE;
}

static LogPipe *
maxminddb_parser_clone(LogPipe *s)
{
  GeoIPParser *self = (GeoIPParser *) s;
  GeoIPParser *cloned;

  cloned = (GeoIPParser *) maxminddb_parser_new(s->cfg);

  geoip_parser_set_database_path(&cloned->super, self->database_path);
  geoip_parser_set_prefix(&cloned->super, self->prefix);
  log_parser_set_template(&cloned->super, log_template_ref(self->super.template));

  return &cloned->super.super;
}

static void
maxminddb_parser_free(LogPipe *s)
{
  GeoIPParser *self = (GeoIPParser *) s;

  g_free(self->database_path);
  g_free(self->prefix);
  if (self->database)
    {
      MMDB_close(self->database);
      g_free(self->database);
    }

  log_parser_free_method(s);
}

static void
remove_trailing_dot(gchar *str)
{
  if (!strlen(str))
    return;
  if (str[strlen(str)-1] == '.')
    str[strlen(str)-1] = 0;
}

static gboolean
maxminddb_parser_init(LogPipe *s)
{
  GeoIPParser *self = (GeoIPParser *) s;

  if (!self->database_path)
    return FALSE;

  self->database = g_new0(MMDB_s, 1);
  if (!self->database)
    return FALSE;

  if (!mmdb_open_database(self->database_path, self->database))
    return FALSE;

  remove_trailing_dot(self->prefix);

  return log_parser_init_method(s);
}

LogParser *
maxminddb_parser_new(GlobalConfig *cfg)
{
  GeoIPParser *self = g_new0(GeoIPParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.init = maxminddb_parser_init;
  self->super.super.free_fn = maxminddb_parser_free;
  self->super.super.clone = maxminddb_parser_clone;
  self->super.process = maxminddb_parser_process;

  geoip_parser_set_prefix(&self->super, ".geoip2");

  return &self->super;
}
