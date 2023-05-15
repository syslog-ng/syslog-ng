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
#include "timeutils/format.h"

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
    date_parser_set_formats(parser, g_list_append(NULL, g_strdup(format)));
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
  setlocale (LC_ALL, "C");
  setenv("TZ", "CET-1", TRUE);
  app_startup();

  configuration = cfg_new_snippet();


  /* year heuristics depends on the current time */

  /* Dec  30 2015 */
  struct timespec faked_time =
  {
    .tv_sec = 1451473200,
    .tv_nsec = 0
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

    /* %Y-%m-%d %H:%M:%S %z */
    { "2015-01-26 00:40:07 PDT", NULL, "%Y-%m-%d %H:%M:%S %z", LM_TS_STAMP, "2015-01-26T00:40:07-07:00" },
    { "2015-01-26 00:40:07 EDT", NULL, "%Y-%m-%d %H:%M:%S %z", LM_TS_STAMP, "2015-01-26T00:40:07-04:00" },
    { "2015-01-26 00:40:07 CET", NULL, "%Y-%m-%d %H:%M:%S %z", LM_TS_STAMP, "2015-01-26T00:40:07+01:00" },

    //The same test as above but using %Z instead of %z
    /* RFC 2822 */
    { "Tue, 27 Jan 2015 11:48:46 +0200", NULL, "%a, %d %b %Y %T %Z", LM_TS_STAMP, "2015-01-27T11:48:46+02:00" },

    /* Apache-like */
    { "21/Jan/2015:14:40:07 +0500", NULL, "%d/%b/%Y:%T %Z", LM_TS_STAMP, "2015-01-21T14:40:07+05:00" },

    /* Try without the year. */
    { "01/Jan:00:40:07 +0500", NULL, "%d/%b:%T %Z", LM_TS_STAMP, "2016-01-01T00:40:07+05:00" },
    { "01/Aug:00:40:07 +0500", NULL, "%d/%b:%T %Z", LM_TS_STAMP, "2015-08-01T00:40:07+05:00" },
    { "01/Sep:00:40:07 +0500", NULL, "%d/%b:%T %Z", LM_TS_STAMP, "2015-09-01T00:40:07+05:00" },
    { "01/Oct:00:40:07 +0500", NULL, "%d/%b:%T %Z", LM_TS_STAMP, "2015-10-01T00:40:07+05:00" },
    { "01/Nov:00:40:07 +0500", NULL, "%d/%b:%T %Z", LM_TS_STAMP, "2015-11-01T00:40:07+05:00" },


    { "1446128356 +01:00", NULL, "%s %z", LM_TS_STAMP, "2015-10-29T15:19:16+01:00" },
    { "1446128356", "Europe/Budapest", "%s", LM_TS_STAMP, "2015-10-29T15:19:16+01:00" },

    /* %Y-%m-%d %H:%M:%S %Z */
    { "2015-01-26 00:40:07 PDT", NULL, "%Y-%m-%d %H:%M:%S %Z", LM_TS_STAMP, "2015-01-26T00:40:07-07:00" },
    { "2015-01-26 00:40:07 EDT", NULL, "%Y-%m-%d %H:%M:%S %Z", LM_TS_STAMP, "2015-01-26T00:40:07-04:00" },
    { "2015-01-26 00:40:07 CET", NULL, "%Y-%m-%d %H:%M:%S %Z", LM_TS_STAMP, "2015-01-26T00:40:07+01:00" },

    /* Try with different missing fields*/
    { "10:30:00 PDT", NULL, "%H:%M:%S %Z", LM_TS_STAMP, "2015-12-30T10:30:00-07:00" },
    { "03-17 10:30:00 PDT", NULL, "%m-%d %H:%M:%S %Z", LM_TS_STAMP, "2015-03-17T10:30:00-07:00" },
    { "03 10:30:00 PDT", NULL, "%m %H:%M:%S %Z", LM_TS_STAMP, "2015-03-01T10:30:00-07:00" },
    { "2015-03 10:30:00 EDT", NULL, "%Y-%m %H:%M:%S %Z", LM_TS_STAMP, "2015-03-01T10:30:00-04:00" },
    { "2015-03-01 EDT", NULL, "%Y-%m-%d %Z", LM_TS_STAMP, "2015-03-01T00:00:00-04:00" },
    { "2015-03 EDT", NULL, "%Y-%m %Z", LM_TS_STAMP, "2015-03-01T00:00:00-04:00" },
    { "2015-03-01 10:30 EDT", NULL, "%Y-%m-%d %H:%M %Z", LM_TS_STAMP, "2015-03-01T10:30:00-04:00" },

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

  append_format_unix_time(&logmsg->timestamps[params->time_stamp], res, TS_FMT_ISO, -1, 0);

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

struct date_with_multiple_formats_params
{
  const gchar *msg;
  int expected_usec;
};

ParameterizedTestParameters(date, test_date_with_multiple_formats)
{
  static struct date_with_multiple_formats_params params[] =
  {
    { "2017-02-02 00:29:16",                0 },
    { "2017-02-02 00:29:16,706",       706000 },
    { "2019-05-04T21:55:46.989+02:00", 989000 },
  };

  return cr_make_param_array(struct date_with_multiple_formats_params, params,
                             sizeof (params) / sizeof(struct date_with_multiple_formats_params));
}

ParameterizedTest(struct date_with_multiple_formats_params *params, date, test_date_with_multiple_formats)
{
  LogParser *parser;
  GList *formats;
  formats = g_list_prepend(NULL, g_strdup("%FT%T.%f%z"));
  formats = g_list_prepend(formats, g_strdup("%F %T,%f"));
  formats = g_list_prepend(formats, g_strdup("%F %T"));

  parser = date_parser_new (configuration);
  date_parser_set_formats(parser, formats);
  date_parser_set_time_stamp(parser, LM_TS_STAMP);

  LogMessage *logmsg = _construct_logmsg(params->msg);

  gboolean success = log_parser_process(parser, &logmsg, NULL, log_msg_get_value(logmsg, LM_V_MESSAGE, NULL), -1);

  cr_assert(success, "unable to parse msg=%s with a list of formats", params->msg);

  cr_assert(logmsg->timestamps[LM_TS_STAMP].ut_usec == params->expected_usec, "expected %d us, got %d",
            params->expected_usec,
            logmsg->timestamps[LM_TS_STAMP].ut_usec);
  log_msg_unref(logmsg);

  log_pipe_unref(&parser->super);
}

Test(date, test_date_with_guess_timezone)
{
  const gchar *msg = "2015-12-30T12:00:00+05:00";
  GString *res = g_string_sized_new(128);

  LogParser *parser = _construct_parser(NULL, NULL, LM_TS_STAMP);
  date_parser_process_flag(parser, "guess-timezone");

  LogMessage *logmsg = _construct_logmsg(msg);
  gboolean success = log_parser_process(parser, &logmsg, NULL, log_msg_get_value(logmsg, LM_V_MESSAGE, NULL), -1);

  cr_assert(success, "failed to parse timestamp, msg=%s", msg);
  append_format_unix_time(&logmsg->timestamps[LM_TS_STAMP], res, TS_FMT_ISO, -1, 0);

  /* this should fix up the timezone */
  cr_assert_str_eq(res->str, "2015-12-30T12:00:00+01:00",
                   "incorrect date parsed msg=%s result=%s",
                   msg, res->str);

  log_pipe_unref(&parser->super);
  log_msg_unref(logmsg);
  g_string_free(res, TRUE);
}
