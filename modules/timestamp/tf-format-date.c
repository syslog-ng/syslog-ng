/*
 * Copyright (c) 2022 Balazs Scheidler <bazsi77@gmail.com>
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
#include "tf-format-date.h"
#include "timeutils/conv.h"


/*
 * $(format-date [options] format-string [timestamp])
 *
 * $(format-date) takes a timestamp in the DATETIME representation and
 * formats it according to an strftime() format string.  The DATETIME
 * representation in syslog-ng is a UNIX timestamp formatted as a decimal
 * number, with an optional fractional part, where the seconds and the
 * fraction of seconds are separated by a dot.
 *
 * If the timestamp argument is missing, the timestamp of the message is
 * used.
 *
 * Options:
 *   --time-zone <TZstring>
 */
typedef struct _TFFormatDate
{
  TFSimpleFuncState super;
  TimeZoneInfo *time_zone_info;
  gchar *format;
} TFFormatDate;

static gboolean
tf_format_date_prepare(LogTemplateFunction *self, gpointer s, LogTemplate *parent, gint argc, gchar *argv[],
                       GError **error)
{
  TFFormatDate *state = (TFFormatDate *) s;
  gchar *time_zone = NULL;
  GOptionContext *ctx;
  GOptionEntry format_date_options[] =
  {
    { "time-zone", 'Z', 0, G_OPTION_ARG_STRING, &time_zone, NULL, NULL },
    { NULL }
  };
  gboolean result = FALSE;

  ctx = g_option_context_new("format-date");
  g_option_context_add_main_entries(ctx, format_date_options, NULL);

  if (!g_option_context_parse(ctx, &argc, &argv, error))
    {
      g_option_context_free(ctx);
      goto exit;
    }
  g_option_context_free(ctx);

  if (time_zone)
    {
      state->time_zone_info = time_zone_info_new(time_zone);
      if (!state->time_zone_info)
        {
          g_set_error(error, LOG_TEMPLATE_ERROR, LOG_TEMPLATE_ERROR_COMPILE,
                      "$(format-date): Error loading timezone information for %s", time_zone);
          goto exit;
        }
    }

  if (argc > 1)
    {
      /* take off format string if that our next argument */
      state->format = g_strdup(argv[1]);

      /* fill up slot in argv array */
      argv[1] = argv[0];
      argv++;
      argc--;
    }

  /* skip the extracted format argument */
  if (!tf_simple_func_prepare(self, state, parent, argc, argv, error))
    {
      goto exit;
    }
  result = TRUE;

exit:
  /* glib supplies us with duplicated strings that we are responsible for! */
  g_free(time_zone);
  return result;
}

static void
tf_format_date_call(LogTemplateFunction *self, gpointer s, const LogTemplateInvokeArgs *args, GString *result,
                    LogMessageValueType *type)
{
  TFFormatDate *state = (TFFormatDate *) s;
  UnixTime ut;

  *type = LM_VT_STRING;

  if (state->super.argc != 0)
    {
      const gchar *ts = args->argv[0]->str;
      if (!type_cast_to_datetime_unixtime(ts, -1, &ut, NULL))
        {
          ut.ut_sec = 0;
          ut.ut_usec = 0;
          ut.ut_gmtoff = -1;
        }
    }
  else
    {
      ut = args->messages[0]->timestamps[LM_TS_STAMP];
    }

  WallClockTime wct;

  gint tz_override = state->time_zone_info ? time_zone_info_get_offset(state->time_zone_info, ut.ut_sec) : -1;
  convert_unix_time_to_wall_clock_time_with_tz_override(&ut, &wct, tz_override);

  gchar buf[64];
  gint len = strftime(buf, sizeof(buf), state->format, &wct.tm);

  if (len > 0)
    g_string_append_len(result, buf, len);
}

static void
tf_format_date_free_state(gpointer s)
{
  TFFormatDate *state = (TFFormatDate *) s;

  g_free(state->format);
  if (state->time_zone_info)
    time_zone_info_free(state->time_zone_info);
  tf_simple_func_free_state(&state->super);
}

TEMPLATE_FUNCTION(TFFormatDate, tf_format_date, tf_format_date_prepare, tf_simple_func_eval, tf_format_date_call,
                  tf_format_date_free_state, NULL);
