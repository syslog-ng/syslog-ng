#include "syslog-ng.h"
#include "logmsg.h"
#include "templates.h"
#include "misc.h"
#include "apphook.h"
#include "cfg.h"
#include "timeutils.h"
#include "plugin.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

gboolean success = TRUE;
gboolean verbose = FALSE;
MsgFormatOptions parse_options;

void
testcase(LogMessage *msg, gchar *template, gchar *expected)
{
  LogTemplate *templ;
  GString *res = g_string_sized_new(128);
  GError *error = NULL;

  templ = log_template_new(configuration, "dummy", template);
  if (!log_template_compile(templ, &error))
    {
      fprintf(stderr, "FAIL: error compiling template, template=%s, error=%s\n", template, error->message);
      g_clear_error(&error);
      success = FALSE;
      goto error;
    }
  log_template_format(templ, msg, NULL, LTZ_LOCAL, 0, res);

  if (strcmp(res->str, expected) != 0)
    {
      fprintf(stderr, "FAIL: template test failed, template=%s, [%s] <=> [%s]\n", template, res->str, expected);
      success = FALSE;
    }
  else if (verbose)
    {
      fprintf(stderr, "PASS: template test success, template=%s => %s\n", template, expected);
    }
 error:
  log_template_unref(templ);
  g_string_free(res, TRUE);
}

void
testcase_failure(gchar *template, const gchar *expected_error)
{
  LogTemplate *templ;
  GError *error = NULL;

  templ = log_template_new(configuration, NULL, template);
  if (log_template_compile(templ, &error))
    {
      fprintf(stderr, "FAIL: compilation failure expected to template, but success was returned, template=%s, expected_error=%s\n", template, expected_error);
      success = FALSE;
      goto error;
    }
  if (strstr(error->message, expected_error) == NULL)
    {
      fprintf(stderr, "FAIL: compilation error doesn't match, error=%s, expected_error=%s\n", error->message, expected_error);
      success = FALSE;
      goto error;
    }
 error:
  log_template_unref(templ);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  LogMessage *msg;
  char *msg_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";

  if (argc > 1)
    verbose = TRUE;

  configuration = cfg_new(0x0201);
  plugin_load_module("syslogformat", configuration, NULL);
  plugin_load_module("builtin_tmpl_func", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
  configuration->template_options.frac_digits = 3;
  configuration->template_options.time_zone_info[LTZ_LOCAL] = time_zone_info_new(NULL);

  app_startup();

  putenv("TZ=MET-1METDST");
  tzset();

  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.10.10.10", 1010), &parse_options);
  log_msg_set_value(msg, log_msg_get_value_handle("APP.VALUE"), "value", -1);
  log_msg_set_match(msg, 0, "whole-match", -1);
  log_msg_set_match(msg, 1, "first-match", -1);
  log_msg_set_tag_by_name(msg, "alma");
  log_msg_set_tag_by_name(msg, "korte");
  log_msg_set_tag_by_name(msg, "citrom");

  /* fix some externally or automatically defined values */
  log_msg_set_value(msg, LM_V_HOST_FROM, "kismacska", -1);
  msg->timestamps[LM_TS_RECVD].time.tv_sec = 1139684315;
  msg->timestamps[LM_TS_RECVD].time.tv_usec = 639000;
  msg->timestamps[LM_TS_RECVD].zone_offset = get_local_timezone_ofs(1139684315);

  /* pri 3, fac 19 == local3 */

  testcase(msg, "$FACILITY", "local3");
  testcase(msg, "$FACILITY_NUM", "19");
  testcase(msg, "$PRIORITY", "err");
  testcase(msg, "$LEVEL", "err");
  testcase(msg, "$LEVEL_NUM", "3");
  testcase(msg, "$TAG", "9b");
  testcase(msg, "$TAGS", "alma,korte,citrom");
  testcase(msg, "$PRI", "155");
  testcase(msg, "$DATE", "Feb 11 10:34:56.000");
  testcase(msg, "$FULLDATE", "2006 Feb 11 10:34:56.000");
  testcase(msg, "$ISODATE", "2006-02-11T10:34:56.000+01:00");
  testcase(msg, "$STAMP", "Feb 11 10:34:56.000");
  testcase(msg, "$YEAR", "2006");
  testcase(msg, "$YEAR_DAY", "042");
  testcase(msg, "$MONTH", "02");
  testcase(msg, "$MONTH_WEEK", "1");
  testcase(msg, "$MONTH_ABBREV", "Feb");
  testcase(msg, "$MONTH_NAME", "February");
  testcase(msg, "$DAY", "11");
  testcase(msg, "$HOUR", "10");
  testcase(msg, "$MIN", "34");
  testcase(msg, "$SEC", "56");
  testcase(msg, "$WEEKDAY", "Sat");
  testcase(msg, "$WEEK_DAY", "7");
  testcase(msg, "$WEEK_DAY_NAME", "Saturday");
  testcase(msg, "$WEEK_DAY_ABBREV", "Sat");
  testcase(msg, "$WEEK", "06");
  testcase(msg, "$UNIXTIME", "1139650496.000");
  testcase(msg, "$TZOFFSET", "+01:00");
  testcase(msg, "$TZ", "+01:00");
  testcase(msg, "$R_DATE", "Feb 11 19:58:35.639");
  testcase(msg, "$R_FULLDATE", "2006 Feb 11 19:58:35.639");
  testcase(msg, "$R_ISODATE", "2006-02-11T19:58:35.639+01:00");
  testcase(msg, "$R_STAMP", "Feb 11 19:58:35.639");
  testcase(msg, "$R_YEAR", "2006");
  testcase(msg, "$R_YEAR_DAY", "042");
  testcase(msg, "$R_MONTH", "02");
  testcase(msg, "$R_MONTH_WEEK", "1");
  testcase(msg, "$R_MONTH_ABBREV", "Feb");
  testcase(msg, "$R_MONTH_NAME", "February");
  testcase(msg, "$R_DAY", "11");
  testcase(msg, "$R_HOUR", "19");
  testcase(msg, "$R_MIN", "58");
  testcase(msg, "$R_SEC", "35");
  testcase(msg, "$R_WEEKDAY", "Sat");
  testcase(msg, "$R_WEEK_DAY", "7");
  testcase(msg, "$R_WEEK_DAY_NAME", "Saturday");
  testcase(msg, "$R_WEEK_DAY_ABBREV", "Sat");
  testcase(msg, "$R_WEEK", "06");
  testcase(msg, "$R_UNIXTIME", "1139684315.639");
  testcase(msg, "$R_TZOFFSET", "+01:00");
  testcase(msg, "$R_TZ", "+01:00");
  testcase(msg, "$S_DATE", "Feb 11 10:34:56.000");
  testcase(msg, "$S_FULLDATE", "2006 Feb 11 10:34:56.000");
  testcase(msg, "$S_ISODATE", "2006-02-11T10:34:56.000+01:00");
  testcase(msg, "$S_STAMP", "Feb 11 10:34:56.000");
  testcase(msg, "$S_YEAR", "2006");
  testcase(msg, "$S_YEAR_DAY", "042");
  testcase(msg, "$S_MONTH", "02");
  testcase(msg, "$S_MONTH_WEEK", "1");
  testcase(msg, "$S_MONTH_ABBREV", "Feb");
  testcase(msg, "$S_MONTH_NAME", "February");
  testcase(msg, "$S_DAY", "11");
  testcase(msg, "$S_HOUR", "10");
  testcase(msg, "$S_MIN", "34");
  testcase(msg, "$S_SEC", "56");
  testcase(msg, "$S_WEEKDAY", "Sat");
  testcase(msg, "$S_WEEK_DAY", "7");
  testcase(msg, "$S_WEEK_DAY_NAME", "Saturday");
  testcase(msg, "$S_WEEK_DAY_ABBREV", "Sat");
  testcase(msg, "$S_WEEK", "06");
  testcase(msg, "$S_UNIXTIME", "1139650496.000");
  testcase(msg, "$S_TZOFFSET", "+01:00");
  testcase(msg, "$S_TZ", "+01:00");
  testcase(msg, "$HOST_FROM", "kismacska");
  testcase(msg, "$FULLHOST_FROM", "kismacska");
  testcase(msg, "$HOST", "bzorp");
  testcase(msg, "$FULLHOST", "bzorp");
  testcase(msg, "$PROGRAM", "syslog-ng");
  testcase(msg, "$PID", "23323");
  testcase(msg, "$MSGHDR", "syslog-ng[23323]:");
  testcase(msg, "$MSG", "syslog-ng[23323]:árvíztűrőtükörfúrógép");
  testcase(msg, "$MSGONLY", "árvíztűrőtükörfúrógép");
  testcase(msg, "$MESSAGE", "syslog-ng[23323]:árvíztűrőtükörfúrógép");
  testcase(msg, "$SOURCEIP", "10.10.10.10");
  testcase(msg, "$PROGRAM/var/log/messages/$HOST/$HOST_FROM/$MONTH$DAY${QQQQQ}valami", "syslog-ng/var/log/messages/bzorp/kismacska/0211valami");
  testcase(msg, "${APP.VALUE}", "value");
  testcase(msg, "${APP.VALUE:-ures}", "value");
  testcase(msg, "${APP.VALUE2:-ures}", "ures");
  testcase(msg, "${1}", "first-match");
  testcase(msg, "$1", "first-match");
  testcase(msg, "$$$1$$", "$first-match$");

  /* template functions */
  testcase(msg, "$(echo $HOST $PID)", "bzorp 23323");
  testcase(msg, "$(echo \"$(echo $HOST)\" $PID)", "bzorp 23323");
  testcase(msg, "$(echo \"$(echo '$(echo $HOST)')\" $PID)", "bzorp 23323");
  testcase(msg, "$(echo \"$(echo '$(echo $HOST)')\" $PID)", "bzorp 23323");
  testcase(msg, "$(echo '\"$(echo $(echo $HOST))\"' $PID)", "\"bzorp\" 23323");

  /* template syntax errors */
  testcase_failure("${unbalanced_brace", "'}' is missing");
  testcase(msg, "$unbalanced_brace}", "}");
  testcase(msg, "$}", "$}");
  testcase_failure("$(unbalanced_paren", "missing function name or inbalanced '('");
  testcase(msg, "$unbalanced_paren)", ")");

  configuration->version = 0x0300;
  testcase(msg, "$MSGHDR", "syslog-ng[23323]:");
  testcase(msg, "$MSG", "árvíztűrőtükörfúrógép");
  testcase(msg, "$MESSAGE", "árvíztűrőtükörfúrógép");

  log_msg_unref(msg);

  msg_str = "syslog-ng: árvíztűrőtükörfúrógép [pid test]";
  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.10.10.10", 1010), &parse_options);

  testcase(msg, "$PID", "");
  log_msg_unref(msg);

  msg_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";

  parse_options.flags = LP_EXPECT_HOSTNAME;
  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.10.10.10", 1010), &parse_options);

  testcase(msg, "$LEGACY_MSGHDR", "");
  testcase(msg, "$MSGHDR", "syslog-ng[23323]: ");
  log_msg_unref(msg);

  msg_str = "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog 3535 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...";
  parse_options.flags = LP_SYSLOG_PROTOCOL;
  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.10.10.10", 1010), &parse_options);

  testcase(msg, "$PRI", "132");
  testcase(msg, "$HOST", "mymachine");
  testcase(msg, "$PROGRAM", "evntslog");
  testcase(msg, "$PID", "3535");
  testcase(msg, "$MSGID", "ID47");
  testcase(msg, "${.SDATA.exampleSDID@0.iut}", "3");
  testcase(msg, "${.SDATA.exampleSDID@0.eventSource}", "Application");
  testcase(msg, "${.SDATA.exampleSDID@0.eventID}", "1011");
  testcase(msg, "${.SDATA.examplePriority@0.class}", "high");


  app_shutdown();

  if (success)
    return 0;
  return 1;
}
