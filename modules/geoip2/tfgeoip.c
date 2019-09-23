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

#include "plugin.h"
#include "plugin-types.h"
#include "template/simple-function.h"
#include "messages.h"
#include "cfg.h"

#include "syslog-ng-config.h"
#include "maxminddb-helper.h"
#include "geoip-parser.h"

typedef struct
{
  TFSimpleFuncState super;
  MMDB_s  *database;
  gchar *database_path;
  gchar **entry_path;
} TFMaxMindDBState;

static inline gboolean
tf_maxminddb_init(TFMaxMindDBState *state)
{
  state->database = g_new0(MMDB_s, 1);

  if (!mmdb_open_database(state->database_path, state->database))
    return FALSE;

  return TRUE;
}

static gboolean
tf_geoip_maxminddb_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent,
                           gint argc, gchar *argv[], GError **error)
{
  TFMaxMindDBState *state = (TFMaxMindDBState *) s;
  gchar *field = NULL;
  state->database_path = NULL;

  GOptionEntry maxminddb_options[] =
  {
    { "database", 'd', 0, G_OPTION_ARG_FILENAME, &state->database_path, "mmdb database location", NULL },
    { "field", 'f', 0, G_OPTION_ARG_STRING, &field, "data path in database. For example: country.iso_code", NULL },
    { NULL }
  };

  GOptionContext *ctx = g_option_context_new("geoip2");
  g_option_context_add_main_entries(ctx, maxminddb_options, NULL);

  if (!g_option_context_parse(ctx, &argc, &argv, error))
    {
      g_option_context_free(ctx);
      return FALSE;
    }
  g_option_context_free(ctx);

  if (!state->database_path)
    state->database_path = mmdb_default_database();

  if (!state->database_path || argc < 1)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "geoip2: format must be: $(geoip2 --database <db.mmdb> [ --field path.child ] ${HOST})\n");
      goto error;
    }

  if (field)
    state->entry_path = g_strsplit(field, ".", -1);
  else
    state->entry_path = g_strsplit("country.iso_code", ".", -1);

  if (!tf_simple_func_prepare(self, state, parent, argc, argv, error))
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "geoip2: prepare failed");
      goto error;
    }

  if (!tf_maxminddb_init(state))
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "geoip2: could not init database");
      goto error;
    }

  return TRUE;

error:
  g_free(state->database_path);
  g_strfreev(state->entry_path);
  g_free(field);
  return FALSE;

}

static void
tf_geoip_maxminddb_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result)
{
  TFMaxMindDBState *state = (TFMaxMindDBState *) s;

  int _gai_error, mmdb_error;
  MMDB_lookup_result_s mmdb_result =
    MMDB_lookup_string(state->database, args->argv[0]->str, &_gai_error, &mmdb_error);

  if (!mmdb_result.found_entry)
    {
      goto error;
    }

  MMDB_entry_data_s entry_data;
  mmdb_error = MMDB_aget_value(&mmdb_result.entry, &entry_data, (const char *const* const)state->entry_path);
  if (mmdb_error != MMDB_SUCCESS)
    {
      goto error;
    }

  if (entry_data.has_data)
    append_mmdb_entry_data_to_gstring(result, &entry_data);

  return;

error:
  if (_gai_error != 0)
    msg_error("$(geoip2): getaddrinfo failed",
              evt_tag_str("ip", args->argv[0]->str),
              evt_tag_str("gai_error", gai_strerror(_gai_error)));

  if (mmdb_error != MMDB_SUCCESS )
    msg_error("$(geoip2): maxminddb error",
              evt_tag_str("ip", args->argv[0]->str),
              evt_tag_str("error", MMDB_strerror(mmdb_error)));
}

static void
tf_geoip_maxminddb_free_state(gpointer s)
{
  TFMaxMindDBState *state = (TFMaxMindDBState *) s;

  g_free(state->database_path);
  g_strfreev(state->entry_path);
  tf_simple_func_free_state(&state->super);
}

TEMPLATE_FUNCTION(TFMaxMindDBState, tf_geoip_maxminddb, tf_geoip_maxminddb_prepare,
                  tf_simple_func_eval, tf_geoip_maxminddb_call, tf_geoip_maxminddb_free_state, NULL);
