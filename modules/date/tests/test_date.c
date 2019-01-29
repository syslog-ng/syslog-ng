/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Vincent Bernat <Vincent.Bernat@exoscale.ch>
 * Copyright (c) 2015 Balázs Scheidler <balazs.scheidler@balabit.com>
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

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include "date-parser.h"
#include "apphook.h"
#include "timeutils/cache.h"

#include <locale.h>
#include <stdlib.h>

struct date_params
{
  gchar *msg;
  gchar *timezone_;
  gchar *format;
  gint time_stamp;
  gchar *expected;
};

static LogParser *
_construct_parser(gchar *timezone_, gchar *format, gint time_stamp)
{
  LogParser *parser;

  parser = date_parser_new (configuration);
  if (format != NULL)
    date_parser_set_format(parser, format);
  if (timezone_ != NULL)
    date_parser_set_timezone(parser, timezone_);
  date_parser_set_time_stamp(parser, time_stamp);

  log_pipe_init(&parser->super);
  return parser;
}

static LogMessage *
_construct_logmsg(const gchar *msg)
{
  LogMessage *logmsg;

  logmsg = log_msg_new_empty();
  log_msg_set_value(logmsg, LM_V_MESSAGE, msg, -1);
  return logmsg;
}

void
setup(void)
{
  app_startup();

  setlocale (LC_ALL, "C");
  setenv("TZ", "CET-1", TRUE);
  tzset();

  configuration = cfg_new_snippet();


  /* year heuristics depends on the current time */

  /* Dec  30 2015 */
  GTimeVal faked_time =
  {
    .tv_sec = 1451473200,
    .tv_usec = 0
  };
  set_cached_time(&faked_time);
}

void
teardown(void)
{
  app_shutdown();
}

TestSuite(date, .init = setup, .fini = teardown);

ParameterizedTestParameters(date, test_date_parser)
{
  static struct date_params params[] =
  {
    { "2015-01-26T16:14:49+03:00", NULL, NULL, LM_TS_RECVD, "2015-01-26T16:14:49+03:00" },

    /* Various ISO8601 formats */
    { "2015-01-26T16:14:49+0300", NULL, NULL, LM_TS_STAMP, "2015-01-26T16:14:49+03:00" },
    { "2015-01-26T16:14:49+0330", NULL, NULL, LM_TS_STAMP, "2015-01-26T16:14:49+03:30" },
    { "2015-01-26T16:14:49+0200", NULL, NULL, LM_TS_STAMP, "2015-01-26T16:14:49+02:00" },
    { "2015-01-26T16:14:49+03:00", NULL, NULL, LM_TS_STAMP, "2015-01-26T16:14:49+03:00" },
    { "2015-01-26T16:14:49+03:30", NULL, NULL, LM_TS_STAMP, "2015-01-26T16:14:49+03:30" },
    { "2015-01-26T16:14:49+02:00", NULL, NULL, LM_TS_STAMP, "2015-01-26T16:14:49+02:00" },
    { "2015-01-26T16:14:49Z", NULL, NULL, LM_TS_STAMP, "2015-01-26T16:14:49+00:00" },
    { "2015-01-26T16:14:49A", NULL, NULL, LM_TS_STAMP, "2015-01-26T16:14:49-01:00" },
    { "2015-01-26T16:14:49B", NULL, NULL, LM_TS_STAMP, "2015-01-26T16:14:49-02:00" },
    { "2015-01-26T16:14:49N", NULL, NULL, LM_TS_STAMP, "2015-01-26T16:14:49+01:00" },
    { "2015-01-26T16:14:49O", NULL, NULL, LM_TS_STAMP, "2015-01-26T16:14:49+02:00" },
    { "2015-01-26T16:14:49GMT", NULL, NULL, LM_TS_STAMP, "2015-01-26T16:14:49+00:00" },
    { "2015-01-26T16:14:49PDT", NULL, NULL, LM_TS_STAMP, "2015-01-26T16:14:49-07:00" },

    /* RFC 2822 */
    { "Tue, 27 Jan 2015 11:48:46 +0200", NULL, "%a, %d %b %Y %T %z", LM_TS_STAMP, "2015-01-27T11:48:46+02:00" },

    /* Apache-like */
    { "21/Jan/2015:14:40:07 +0500", NULL, "%d/%b/%Y:%T %z", LM_TS_STAMP, "2015-01-21T14:40:07+05:00" },

    /* Dates without timezones. America/Phoenix has no DST */
    { "Tue, 27 Jan 2015 11:48:46", NULL, "%a, %d %b %Y %T", LM_TS_STAMP, "2015-01-27T11:48:46+01:00" },
    { "Tue, 27 Jan 2015 11:48:46", "America/Phoenix", "%a, %d %b %Y %T", LM_TS_STAMP, "2015-01-27T11:48:46-07:00" },
    { "Tue, 27 Jan 2015 11:48:46", "+05:00", "%a, %d %b %Y %T", LM_TS_STAMP, "2015-01-27T11:48:46+05:00" },

    /* Try without the year. */
    { "01/Jan:00:40:07 +0500", NULL, "%d/%b:%T %z", LM_TS_STAMP, "2016-01-01T00:40:07+05:00" },
    { "01/Aug:00:40:07 +0500", NULL, "%d/%b:%T %z", LM_TS_STAMP, "2015-08-01T00:40:07+05:00" },
    { "01/Sep:00:40:07 +0500", NULL, "%d/%b:%T %z", LM_TS_STAMP, "2015-09-01T00:40:07+05:00" },
    { "01/Oct:00:40:07 +0500", NULL, "%d/%b:%T %z", LM_TS_STAMP, "2015-10-01T00:40:07+05:00" },
    { "01/Nov:00:40:07 +0500", NULL, "%d/%b:%T %z", LM_TS_STAMP, "2015-11-01T00:40:07+05:00" },


    { "1446128356 +01:00", NULL, "%s %z", LM_TS_STAMP, "2015-10-29T15:19:16+01:00" },
    { "1446128356", "Europe/Budapest", "%s", LM_TS_STAMP, "2015-10-29T15:19:16+01:00" },
  };

  return cr_make_param_array(struct date_params, params, sizeof(params) / sizeof(struct date_params));
}

ParameterizedTest(struct date_params *params, date, test_date_parser)
{
  LogMessage *logmsg;
  LogParser *parser = _construct_parser(params->timezone_, params->format, params->time_stamp);
  gboolean success;
  GString *res = g_string_sized_new(128);

  logmsg = _construct_logmsg(params->msg);
  success = log_parser_process(parser, &logmsg, NULL, log_msg_get_value(logmsg, LM_V_MESSAGE, NULL), -1);

  cr_assert(success, "unable to parse format=%s msg=%s", params->format, params->msg);

  unix_time_append_format(&logmsg->timestamps[params->time_stamp], res, TS_FMT_ISO, -1, 0);

  cr_assert_str_eq(res->str, params->expected,
                   "incorrect date parsed msg=%s format=%s, result=%s, expected=%s",
                   params->msg, params->format, res->str, params->expected);

  g_string_free(res, TRUE);
  log_pipe_unref(&parser->super);
  log_msg_unref(logmsg);
}

Test(date, test_date_with_additional_text_at_the_end)
{
  const gchar *msg = "2015-01-26T16:14:49+0300 Disappointing log file";

  LogParser *parser = _construct_parser(NULL, NULL, LM_TS_STAMP);
  LogMessage *logmsg = _construct_logmsg(msg);
  gboolean success = log_parser_process(parser, &logmsg, NULL, log_msg_get_value(logmsg, LM_V_MESSAGE, NULL), -1);

  cr_assert_not(success, "successfully parsed but expected failure, msg=%s", msg);

  log_pipe_unref(&parser->super);
  log_msg_unref(logmsg);
}
