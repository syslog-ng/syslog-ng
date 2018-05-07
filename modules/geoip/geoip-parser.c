/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Gergely Nagy
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
#include "parser/parser-expr.h"
#include "scratch-buffers.h"
#include "geoip-helper.h"

typedef struct _GeoIPParser GeoIPParser;

struct _GeoIPParser
{
  LogParser super;
  GeoIP *gi;

  gchar *database;
  gchar *prefix;
  void (*add_geoip_result)(GeoIPParser *, LogMessage *, const gchar *);
  struct
  {
    gchar *country_code;
    gchar *longitude;
    gchar *latitude;
  } dest;
};

void
geoip_parser_set_prefix(LogParser *s, const gchar *prefix)
{
  GeoIPParser *self = (GeoIPParser *) s;

  g_free(self->prefix);
  self->prefix = g_strdup(prefix);
}

static void
geoip_parser_reset_fields(GeoIPParser *self)
{
  g_free(self->dest.country_code);
  self->dest.country_code = g_strdup_printf("%scountry_code", self->prefix);

  g_free(self->dest.longitude);
  self->dest.longitude = g_strdup_printf("%slongitude", self->prefix);

  g_free(self->dest.latitude);
  self->dest.latitude = g_strdup_printf("%slatitude", self->prefix);
}

void
geoip_parser_set_database(LogParser *s, const gchar *database)
{
  GeoIPParser *self = (GeoIPParser *) s;

  g_free(self->database);
  self->database = g_strdup(database);
}

static void
add_geoip_record(GeoIPParser *self, LogMessage *msg, const gchar *ip)
{
  GeoIPRecord *record;
  GString *value;

  record = GeoIP_record_by_name(self->gi, ip);
  if (record)
    {
      if (record->country_code)
        log_msg_set_value_by_name(msg, self->dest.country_code,
                                  record->country_code,
                                  strlen(record->country_code));

      value = scratch_buffers_alloc();

      g_string_printf(value, "%f",
                      (double)record->latitude);
      log_msg_set_value_by_name(msg, self->dest.latitude,
                                value->str,
                                value->len);

      g_string_printf(value, "%f",
                      (double)record->longitude);
      log_msg_set_value_by_name(msg, self->dest.longitude,
                                value->str,
                                value->len);

      GeoIPRecord_delete(record);
    }
}

static void
add_geoip_country_code(GeoIPParser *self, LogMessage *msg, const gchar *ip)
{
  const char *country;

  country = GeoIP_country_code_by_name(self->gi, ip);
  if (country)
    log_msg_set_value_by_name(msg, self->dest.country_code,
                              country,
                              strlen(country));
}

static gboolean
geoip_parser_process(LogParser *s, LogMessage **pmsg,
                     const LogPathOptions *path_options,
                     const gchar *input, gsize input_len)
{
  GeoIPParser *self = (GeoIPParser *) s;
  LogMessage *msg = log_msg_make_writable(pmsg, path_options);

  if (!self->dest.country_code &&
      !self->dest.latitude &&
      !self->dest.longitude)
    return TRUE;

  self->add_geoip_result(self, msg, input);

  return TRUE;
}

static LogPipe *
geoip_parser_clone(LogPipe *s)
{
  GeoIPParser *self = (GeoIPParser *) s;
  GeoIPParser *cloned;

  cloned = (GeoIPParser *) geoip_parser_new(s->cfg);

  geoip_parser_set_database(&cloned->super, self->database);
  geoip_parser_set_prefix(&cloned->super, self->prefix);
  log_parser_set_template(&cloned->super, log_template_ref(self->super.template));
  geoip_parser_reset_fields(cloned);

  return &cloned->super.super;
}

static void
geoip_parser_free(LogPipe *s)
{
  GeoIPParser *self = (GeoIPParser *) s;

  g_free(self->dest.country_code);
  g_free(self->dest.latitude);
  g_free(self->dest.longitude);
  g_free(self->database);
  g_free(self->prefix);

  GeoIP_delete(self->gi);

  log_parser_free_method(s);
}

static gboolean
geoip_parser_init(LogPipe *s)
{
  GeoIPParser *self = (GeoIPParser *) s;

  geoip_parser_reset_fields(self);

  if (self->database)
    self->gi = GeoIP_open(self->database, GEOIP_MMAP_CACHE);
  else
    self->gi = GeoIP_new(GEOIP_MMAP_CACHE);

  if (!self->gi)
    return FALSE;

  if (is_country_type(self->gi->databaseType))
    {
      msg_debug("geoip: country type database detected",
                evt_tag_int("database_type", self->gi->databaseType));
      self->add_geoip_result = add_geoip_country_code;
    }
  else
    {
      msg_debug("geoip: city type database detected",
                evt_tag_int("database_type", self->gi->databaseType));
      self->add_geoip_result = add_geoip_record;
    }
  return log_parser_init_method(s);
}

LogParser *
geoip_parser_new(GlobalConfig *cfg)
{
  GeoIPParser *self = g_new0(GeoIPParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.init = geoip_parser_init;
  self->super.super.free_fn = geoip_parser_free;
  self->super.super.clone = geoip_parser_clone;
  self->super.process = geoip_parser_process;

  geoip_parser_set_prefix(&self->super, ".geoip.");

  return &self->super;
}
