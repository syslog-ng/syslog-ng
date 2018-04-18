/*
 * Copyright (c) 2012-2013 Balabit
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

#include "plugin.h"
#include "plugin-types.h"
#include "template/simple-function.h"
#include "messages.h"
#include "cfg.h"
#include "tls-support.h"

#include "syslog-ng-config.h"
#include "geoip-helper.h"

#include <GeoIP.h>

typedef struct _TFGeoIPState TFGeoIPState;

struct _TFGeoIPState
{
  TFSimpleFuncState super;
  GeoIP *gi;
  gchar *database;
  void (*add_geoip_result)(TFGeoIPState *state, GString *result, const gchar *ip);
};

static void
add_geodata_from_geocity(TFGeoIPState *state, GString *result, const gchar *ip)
{
  GeoIPRecord *record;

  record = GeoIP_record_by_name(state->gi, ip);
  if (record)
    {
      if (record->country_code)
        g_string_append(result, record->country_code);
      GeoIPRecord_delete(record);
    }
}

static void
add_geodata_from_geocountry(TFGeoIPState *state, GString *result, const gchar *ip)
{
  const char *country;
  country = GeoIP_country_code_by_name(state->gi, ip);
  if (country)
    g_string_append(result, country);
}

static inline gboolean
tf_geoip_init(TFGeoIPState *state)
{

  if (state->database)
    state->gi = GeoIP_open(state->database, GEOIP_MMAP_CACHE);
  else
    state->gi = GeoIP_new(GEOIP_MMAP_CACHE);

  if (!state->gi)
    return FALSE;

  if (is_country_type(state->gi->databaseType))
    {
      msg_debug("geoip: country type database detected",
                evt_tag_int("database_type", state->gi->databaseType));
      state->add_geoip_result = add_geodata_from_geocountry;
    }
  else
    {
      msg_debug("geoip: city type database detected",
                evt_tag_int("database_type", state->gi->databaseType));
      state->add_geoip_result = add_geodata_from_geocity;
    }
  return TRUE;
}

static gboolean
tf_geoip_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent,
                 gint argc, gchar *argv[], GError **error)
{
  TFGeoIPState *state = (TFGeoIPState *) s;
  state->database = NULL;

  msg_notice("The geoip template function is deprecated. Please use geoip2 template function instead");

  GOptionEntry geoip_options[] =
  {
    { "database", 'd', 0, G_OPTION_ARG_FILENAME, &state->database, "geoip database location", NULL },
    { NULL }
  };

  GOptionContext *ctx = g_option_context_new("geoip");
  g_option_context_add_main_entries(ctx, geoip_options, NULL);

  if (!g_option_context_parse(ctx, &argc, &argv, error))
    {
      g_option_context_free(ctx);
      return FALSE;
    }
  g_option_context_free(ctx);

  if (argc != 2)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "geoip: format must be: $(geoip [--database <file location>] ${HOST})\n");
      goto error;
    }

  if (!tf_simple_func_prepare(self, state, parent, argc, argv, error))
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "geoip: prepare failed");
      goto error;
    }

  if (!tf_geoip_init(state))
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "geoip: error while opening database");

      goto error;
    }

  return TRUE;

error:
  return FALSE;

}

static void
tf_geoip_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  TFGeoIPState *state = (TFGeoIPState *) s;
  GString **argv = (GString **) args->bufs->pdata;

  state->add_geoip_result(state, result, argv[0]->str);

}

static void
tf_geoip_free_state(gpointer s)
{
  TFGeoIPState *state = (TFGeoIPState *) s;

  if (state->database)
    g_free(state->database);
  tf_simple_func_free_state(&state->super);
}

TEMPLATE_FUNCTION(TFGeoIPState, tf_geoip, tf_geoip_prepare,
                  tf_simple_func_eval, tf_geoip_call, tf_geoip_free_state, NULL);
