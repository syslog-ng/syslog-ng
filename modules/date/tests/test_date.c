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

#include "date-parser.h"
#include "apphook.h"
#include "testutils.h"
#include "template_lib.h"

#include <locale.h>
#include <stdlib.h>

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
  logmsg->timestamps[LM_TS_RECVD].tv_sec = 1451473200; /* Dec  30 2015 */
  log_msg_set_value(logmsg, LM_V_MESSAGE, msg, -1);
  return logmsg;
}

static void
assert_parsed_date_equals_with_stamp(gchar *msg, gchar *timezone_, gchar *format, gint time_stamp, gchar *expected)
{
  LogMessage *logmsg;
  LogParser *parser = _construct_parser(timezone_, format, time_stamp);
  gboolean success;
  GString *res = g_string_sized_new(128);

  logmsg = _construct_logmsg(msg);
  success = log_parser_process(parser, &logmsg, NULL, log_msg_get_value(logmsg, LM_V_MESSAGE, NULL), -1);

  if (!success)
    {
      fprintf(stderr, "unable to parse format=%s msg=%s\n", format, msg);
      exit(1);
    }
  else
    {
      log_stamp_append_format(&logmsg->timestamps[time_stamp], res, TS_FMT_ISO, -1, 0);
      assert_nstring(res->str, res->len, expected, strlen(expected),
                     "incorrect date parsed msg=%s format=%s",
                     msg, format);
    }

  g_string_free(res, TRUE);
  log_pipe_unref(&parser->super);
  log_msg_unref(logmsg);
  return;
}

static void
assert_parsed_date_equals(gchar *msg, gchar *timezone_, gchar *format, gchar *expected)
{
  assert_parsed_date_equals_with_stamp(msg, timezone_, format, LM_TS_STAMP, expected);
}

static void
assert_parsing_fails(gchar *msg)
{
  LogMessage *logmsg;
  LogParser *parser = _construct_parser(NULL, NULL, LM_TS_STAMP);
  gboolean success;

  logmsg = _construct_logmsg(msg);
  success = log_parser_process(parser, &logmsg, NULL, log_msg_get_value(logmsg, LM_V_MESSAGE, NULL), -1);

  if (success)
    {
      fprintf(stderr, "successfully parsed but expected failure, msg=%s\n", msg);
      exit(1);
    }

  log_pipe_unref(&parser->super);
  log_msg_unref(logmsg);
  return;
}


int main()
{
  app_startup();

  setlocale (LC_ALL, "C");
  putenv("TZ=CET-1");
  tzset();

  configuration = cfg_new(0x0302);

  /* Various ISO8601 formats */
  assert_parsed_date_equals("2015-01-26T16:14:49+0300", NULL, NULL, "2015-01-26T16:14:49+03:00");
  assert_parsed_date_equals("2015-01-26T16:14:49+0330", NULL, NULL, "2015-01-26T16:14:49+03:30");
  assert_parsed_date_equals("2015-01-26T16:14:49+0200", NULL, NULL, "2015-01-26T16:14:49+02:00");
  assert_parsed_date_equals("2015-01-26T16:14:49+03:00", NULL, NULL, "2015-01-26T16:14:49+03:00");
  assert_parsed_date_equals("2015-01-26T16:14:49+03:30", NULL, NULL, "2015-01-26T16:14:49+03:30");
  assert_parsed_date_equals("2015-01-26T16:14:49+02:00", NULL, NULL, "2015-01-26T16:14:49+02:00");
  assert_parsed_date_equals("2015-01-26T16:14:49Z", NULL, NULL, "2015-01-26T16:14:49+00:00");
  assert_parsed_date_equals("2015-01-26T16:14:49A", NULL, NULL, "2015-01-26T16:14:49-01:00");
  assert_parsed_date_equals("2015-01-26T16:14:49B", NULL, NULL, "2015-01-26T16:14:49-02:00");
  assert_parsed_date_equals("2015-01-26T16:14:49N", NULL, NULL, "2015-01-26T16:14:49+01:00");
  assert_parsed_date_equals("2015-01-26T16:14:49O", NULL, NULL, "2015-01-26T16:14:49+02:00");
  assert_parsed_date_equals("2015-01-26T16:14:49GMT", NULL, NULL, "2015-01-26T16:14:49+00:00");
  assert_parsed_date_equals("2015-01-26T16:14:49PDT", NULL, NULL, "2015-01-26T16:14:49-07:00");

  /* RFC 2822 */
  assert_parsed_date_equals("Tue, 27 Jan 2015 11:48:46 +0200", NULL, "%a, %d %b %Y %T %z", "2015-01-27T11:48:46+02:00");

  /* Apache-like */
  assert_parsed_date_equals("21/Jan/2015:14:40:07 +0500", NULL, "%d/%b/%Y:%T %z", "2015-01-21T14:40:07+05:00");

  /* Try with additional text at the end, should fail */
  assert_parsing_fails("2015-01-26T16:14:49+0300 Disappointing log file");

  /* Dates without timezones. America/Phoenix has no DST */
  assert_parsed_date_equals("Tue, 27 Jan 2015 11:48:46", NULL, "%a, %d %b %Y %T", "2015-01-27T11:48:46+01:00");
  assert_parsed_date_equals("Tue, 27 Jan 2015 11:48:46", "America/Phoenix", "%a, %d %b %Y %T", "2015-01-27T11:48:46-07:00");
  assert_parsed_date_equals("Tue, 27 Jan 2015 11:48:46", "+05:00", "%a, %d %b %Y %T", "2015-01-27T11:48:46+05:00");

  /* Try without the year. */
  assert_parsed_date_equals("01/Jan:00:40:07 +0500", NULL, "%d/%b:%T %z", "2016-01-01T00:40:07+05:00");
  assert_parsed_date_equals("01/Aug:00:40:07 +0500", NULL, "%d/%b:%T %z", "2015-08-01T00:40:07+05:00");
  assert_parsed_date_equals("01/Sep:00:40:07 +0500", NULL, "%d/%b:%T %z", "2015-09-01T00:40:07+05:00");
  assert_parsed_date_equals("01/Oct:00:40:07 +0500", NULL, "%d/%b:%T %z", "2015-10-01T00:40:07+05:00");
  assert_parsed_date_equals("01/Nov:00:40:07 +0500", NULL, "%d/%b:%T %z", "2015-11-01T00:40:07+05:00");


  assert_parsed_date_equals("1446128356 +01:00", NULL, "%s %z", "2015-10-29T15:19:16+01:00");
  assert_parsed_date_equals("1446128356", "Europe/Budapest", "%s", "2015-10-29T15:19:16+01:00");


  assert_parsed_date_equals_with_stamp("2015-01-26T16:14:49+03:00", NULL, NULL, LM_TS_RECVD, "2015-01-26T16:14:49+03:00");

  app_shutdown();
  return 0;
};
