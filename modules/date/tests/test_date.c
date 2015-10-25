#include "modules/date/date-parser.h"
#include <apphook.h>
#include <libtest/testutils.h>
#include <libtest/template_lib.h>
#include <locale.h>

MsgFormatOptions parse_options;

static void
testcase(gchar *msg, goffset offset, gchar *timezone, gchar *format, gchar *expected)
{
  LogTemplate *templ;
  LogMessage *logmsg;
  NVTable *nvtable;
  GlobalConfig *cfg = cfg_new (0x302);
  LogParser *parser = date_parser_new (cfg);
  gboolean success;
  const gchar *context_id = "test-context-id";
  GString *res = g_string_sized_new(128);

  date_parser_set_offset(parser, offset);
  if (format != NULL) date_parser_set_format(parser, format);
  if (timezone != NULL) date_parser_set_timezone(parser, timezone);

  parse_options.flags = 0;
  logmsg = log_msg_new_empty();
  logmsg->timestamps[LM_TS_RECVD].tv_sec = 1438793384; /* Wed Aug  5 2015 */
  log_msg_set_value(logmsg, log_msg_get_value_handle("MESSAGE"), msg, -1);
  nvtable = nv_table_ref(logmsg->payload);
  success = log_parser_process(parser, &logmsg, NULL, log_msg_get_value(logmsg, LM_V_MESSAGE, NULL), -1);
  nv_table_unref(nvtable);

  if (!success)
    {
      fprintf(stderr, "unable to parse offset=%d format=%s msg=%s\n", offset, format, msg);
      exit(1);
    }

  /* Convert to ISODATE */
  templ = compile_template("${ISODATE}", FALSE);
  log_template_format(templ, logmsg, NULL, LTZ_LOCAL, 999, context_id, res);
  assert_nstring(res->str, res->len, expected, strlen(expected),
                 "incorrect date parsed msg=%s offset=%d format=%s",
                 msg, offset, format);
  log_template_unref(templ);
  g_string_free(res, TRUE);

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
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);

  /* Various ISO8601 formats */
  testcase("2015-01-26T16:14:49+03:00", 0, NULL, NULL, "2015-01-26T16:14:49+03:00");
  // testcase("2015-01-26T16:14:49+03:30", 0, NULL, NULL, "2015-01-26T16:14:49+03:30");
  testcase("2015-01-26T16:14:49+0200", 0, NULL, NULL, "2015-01-26T16:14:49+02:00");
  // testcase("2015-01-26T16:14:49Z", 0, NULL, NULL, "2015-01-26T16:14:49+00:00");

  /* RFC 2822 */
  testcase("Tue, 27 Jan 2015 11:48:46 +0200", 0, NULL, "%a, %d %b %Y %T %z", "2015-01-27T11:48:46+02:00");

  /* Apache-like */
  testcase("21/Jan/2015:14:40:07 +0500", 0, NULL, "%d/%b/%Y:%T %z", "2015-01-21T14:40:07+05:00");

  /* Try with additional text at the end */
  testcase("2015-01-26T16:14:49+03:00 Disappointing log file", 0, NULL, NULL, "2015-01-26T16:14:49+03:00");

  /* Try with offset */
  testcase("<34> 2015-01-26T16:14:49+03:00 Disappointing log file", 5, NULL, NULL, "2015-01-26T16:14:49+03:00");

  /* Dates without timezones. America/Phoenix has no DST */
  testcase("Tue, 27 Jan 2015 11:48:46", 0, NULL, "%a, %d %b %Y %T", "2015-01-27T11:48:46+00:00");
  testcase("Tue, 27 Jan 2015 11:48:46", 0, "America/Phoenix", "%a, %d %b %Y %T", "2015-01-27T11:48:46-07:00");
  testcase("Tue, 27 Jan 2015 11:48:46", 0, "+05:00", "%a, %d %b %Y %T", "2015-01-27T11:48:46+05:00");

  /* Try without the year. */
  testcase("01/Jul:00:40:07 +0500", 0, NULL, "%d/%b:%T %z", "2015-07-01T00:40:07+05:00");
  testcase("01/Aug:00:40:07 +0500", 0, NULL, "%d/%b:%T %z", "2015-08-01T00:40:07+05:00");
  testcase("01/Sep:00:40:07 +0500", 0, NULL, "%d/%b:%T %z", "2015-09-01T00:40:07+05:00");
  testcase("01/Oct:00:40:07 +0500", 0, NULL, "%d/%b:%T %z", "2014-10-01T00:40:07+05:00");
  testcase("01/Nov:00:40:07 +0500", 0, NULL, "%d/%b:%T %z", "2014-11-01T00:40:07+05:00");

  app_shutdown();
  return 0;
};
