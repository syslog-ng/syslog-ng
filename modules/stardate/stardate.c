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

#include <stdlib.h>
#include <math.h>

static gboolean
_is_leap_year(int year)
{
  return ((year%4==0) && (year % 100!=0)) || (year%400==0);
}

static time_t
year_in_seconds(int year)
{
  if (_is_leap_year(year))
    return 31622400;
  else
    return 31536000;
}

typedef struct
{
  TFSimpleFuncState super;
  int precision;
} TFStardateState;

static guint64 power10[10] = { 1, 10, 100, 1000, 10000,
                               100000, 1000000, 10000000, 100000000, 1000000000
                             };

gboolean
tf_stardate_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent,
                    gint argc, gchar *argv[], GError **error)
{
  TFStardateState *state = (TFStardateState *) s;
  state->precision = 2;

  GOptionEntry stardate_options[] =
  {
    { "digits", 'd', 0, G_OPTION_ARG_INT, &state->precision, "Precision: 0-9. Default: 2.", NULL },
    { NULL }
  };

  GOptionContext *ctx = g_option_context_new("stardate");
  g_option_context_add_main_entries(ctx, stardate_options, NULL);

  if (!g_option_context_parse(ctx, &argc, &argv, error))
    {
      g_option_context_free(ctx);
      return FALSE;
    }
  g_option_context_free(ctx);

  if (state->precision < 0 || state->precision > 9)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "stardate: digits must be between 0-9.\n");
      return FALSE;
    }

  if (argc != 2)
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "stardate: format must be: $(stardate [--digits 2] $UNIXTIME)\n");
      return FALSE;
    }

  if (!tf_simple_func_prepare(self, state, parent, argc, argv, error))
    {
      g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                  "stardate: stardate: prepare failed");
      return FALSE;
    }

  return TRUE;
}

static void
tf_stardate_call(LogTemplateFunction *self, gpointer s,
                 const LogTemplateInvokeArgs *args, GString *result)
{
  TFStardateState  *state = (TFStardateState *) s;

  char format[7]; // "%0.xlf\0"
  if (g_snprintf(format, sizeof(format),"%%0.%dlf", state->precision) < 0)
    {
      msg_error("stardate: sprintf error)",
                evt_tag_int("precision", state->precision));
      return;
    }

  char *status;
  time_t time_to_convert = strtol(args->argv[0]->str, &status, 10);
  if (*status)
    {
      msg_error("stardate: wrong format: expected unixtime like value",
                evt_tag_str("got:", args->argv[0]->str));
      return;
    }

  struct tm tm_time;
  localtime_r(&time_to_convert, &tm_time );

  struct tm secs_beginning_year = {.tm_year = tm_time.tm_year, .tm_mday = 1};
  time_t elapsed_seconds = time_to_convert - mktime(&secs_beginning_year);

  double fraction = (double)elapsed_seconds/year_in_seconds(tm_time.tm_year);
  double truncated = (double) floor(fraction * power10[state->precision]) / power10[state->precision];

  g_string_append_printf(result, format, 1900 + tm_time.tm_year + truncated);
}

TEMPLATE_FUNCTION(TFStardateState, tf_stardate, tf_stardate_prepare,
                  tf_simple_func_eval, tf_stardate_call, tf_simple_func_free_state, NULL);
