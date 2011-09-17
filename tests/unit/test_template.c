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
  LogMessage *context[2] = { msg, msg };
  const gchar *context_id = "test-context-id";

  templ = log_template_new(configuration, "dummy");
  if (!log_template_compile(templ, template, &error))
    {
      fprintf(stderr, "FAIL: error compiling template, template=%s, error=%s\n", template, error->message);
      g_clear_error(&error);
      success = FALSE;
      goto error;
    }
  log_template_format_with_context(templ, context, 2, NULL, LTZ_LOCAL, 999, context_id, res);

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

  templ = log_template_new(configuration, NULL);
  if (log_template_compile(templ, template, &error))
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

GCond *thread_ping;
GMutex *thread_lock;
gboolean thread_start;

static gpointer
format_template_thread(gpointer s)
{
  gpointer *args = (gpointer *) s;
  LogMessage *msg = args[0];
  LogTemplate *templ = args[1];
  const gchar *expected = args[2];
  GString *result;
  gint i;

  g_mutex_lock(thread_lock);
  while (!thread_start)
    g_cond_wait(thread_ping, thread_lock);
  g_mutex_unlock(thread_lock);

  result = g_string_sized_new(0);
  for (i = 0; i < 10000; i++)
    {
      log_template_format(templ, msg, NULL, LTZ_SEND, 5555, NULL, result);
      if (strcmp(result->str, expected) != 0)
        {
          fprintf(stderr, "FAIL: multi-threaded formatting yielded invalid result (iteration: %d): %s, expected=%s\n", i, result->str, expected);
          success = FALSE;
          break;
        }
    }
  g_string_free(result, TRUE);
  return NULL;
}

void
testcase_multi_thread(LogMessage *msg, gchar *template, const gchar *expected)
{
  LogTemplate *templ;
  gpointer args[] = { msg, NULL, (gpointer) expected };
  GError *error = NULL;
  GThread *threads[16];
  gint i;

  templ = log_template_new(configuration, NULL);
  if (!log_template_compile(templ, template, &error))
    {
      fprintf(stderr, "FAIL: error compiling template, template=%s, error=%s\n", template, error->message);
      success = FALSE;
      g_clear_error(&error);
      goto error;
    }

  thread_start = FALSE;
  thread_ping = g_cond_new();
  thread_lock = g_mutex_new();
  args[1] = templ;
  for (i = 0; i < 16; i++)
    {
      threads[i] = g_thread_create(format_template_thread, args, TRUE, NULL);
    }

  thread_start = TRUE;
  g_mutex_lock(thread_lock);
  g_cond_broadcast(thread_ping);
  g_mutex_unlock(thread_lock);
  for (i = 0; i < 16; i++)
    {
      g_thread_join(threads[i]);
    }
  g_cond_free(thread_ping);
  g_mutex_free(thread_lock);

 error:
  ;
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  LogMessage *msg;
  char *msg_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";

  if (argc > 1)
    verbose = TRUE;

  configuration = cfg_new(0x0302);
  plugin_load_module("syslogformat", configuration, NULL);
  plugin_load_module("basicfuncs", configuration, NULL);
  plugin_load_module("convertfuncs", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
  configuration->template_options.frac_digits = 3;
  configuration->template_options.time_zone_info[LTZ_LOCAL] = time_zone_info_new(NULL);

  app_startup();

  putenv("TZ=MET-1METDST");
  tzset();

  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.11.12.13", 1010), &parse_options);
  log_msg_set_value(msg, log_msg_get_value_handle("APP.VALUE"), "value", -1);
  log_msg_set_match(msg, 0, "whole-match", -1);
  log_msg_set_match(msg, 1, "first-match", -1);
  log_msg_set_tag_by_name(msg, "alma");
  log_msg_set_tag_by_name(msg, "korte");
  log_msg_clear_tag_by_name(msg, "narancs");
  log_msg_set_tag_by_name(msg, "citrom");

  /* fix some externally or automatically defined values */
  log_msg_set_value(msg, LM_V_HOST_FROM, "kismacska", -1);
  msg->timestamps[LM_TS_RECVD].tv_sec = 1139684315;
  msg->timestamps[LM_TS_RECVD].tv_usec = 639000;
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
  testcase(msg, "$R_HOUR12", "07");
  testcase(msg, "$R_MIN", "58");
  testcase(msg, "$R_SEC", "35");
  testcase(msg, "$R_AMPM", "PM");
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
  testcase(msg, "$S_HOUR12", "10");
  testcase(msg, "$S_MIN", "34");
  testcase(msg, "$S_SEC", "56");
  testcase(msg, "$S_AMPM", "AM");
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
  testcase(msg, "$MSG", "árvíztűrőtükörfúrógép");
  testcase(msg, "$MESSAGE", "árvíztűrőtükörfúrógép");
  testcase(msg, "$SOURCEIP", "10.11.12.13");
  testcase(msg, "$PROGRAM/var/log/messages/$HOST/$HOST_FROM/$MONTH$DAY${QQQQQ}valami", "syslog-ng/var/log/messages/bzorp/kismacska/0211valami");
  testcase(msg, "${APP.VALUE}", "value");
  testcase(msg, "${APP.VALUE:-ures}", "value");
  testcase(msg, "${APP.VALUE2:-ures}", "ures");
  testcase(msg, "${1}", "first-match");
  testcase(msg, "$1", "first-match");
  testcase(msg, "$$$1$$", "$first-match$");

  testcase(msg, "$SEQNUM", "999");
  testcase(msg, "$CONTEXT_ID", "test-context-id");

  /* template functions */
  testcase(msg, "$(echo $HOST $PID)", "bzorp 23323");
  testcase(msg, "$(echo \"$(echo $HOST)\" $PID)", "bzorp 23323");
  testcase(msg, "$(echo \"$(echo '$(echo $HOST)')\" $PID)", "bzorp 23323");
  testcase(msg, "$(echo \"$(echo '$(echo $HOST)')\" $PID)", "bzorp 23323");
  testcase(msg, "$(echo '\"$(echo $(echo $HOST))\"' $PID)", "\"bzorp\" 23323");
  testcase(msg, "$(ipv4-to-int $SOURCEIP)", "168496141");

  testcase(msg, "$(grep 'facility(local3)' $PID)", "23323,23323");
  testcase(msg, "$(grep 'facility(local3)' $PID $PROGRAM)", "23323,syslog-ng,23323,syslog-ng");
  testcase(msg, "$(grep 'facility(local4)' $PID)", "");
  testcase(msg, "$(grep ('$FACILITY' == 'local4') $PID)", "");
  testcase(msg, "$(grep ('$FACILITY(' == 'local3(') $PID)", "23323,23323");
  testcase(msg, "$(grep ('$FACILITY(' == 'local4)') $PID)", "");
  testcase(msg, "$(grep \\'$FACILITY\\'\\ ==\\ \\'local4\\' $PID)", "");
  testcase(msg, "$(if 'facility(local4)' alma korte)", "korte");
  testcase(msg, "$(if 'facility(local3)' alma korte)", "alma");

  testcase(msg, "$(if '\"$FACILITY\" lt \"local3\"' alma korte)", "korte");
  testcase(msg, "$(if '\"$FACILITY\" le \"local3\"' alma korte)", "alma");
  testcase(msg, "$(if '\"$FACILITY\" eq \"local3\"' alma korte)", "alma");
  testcase(msg, "$(if '\"$FACILITY\" ne \"local3\"' alma korte)", "korte");
  testcase(msg, "$(if '\"$FACILITY\" gt \"local3\"' alma korte)", "korte");
  testcase(msg, "$(if '\"$FACILITY\" ge \"local3\"' alma korte)", "alma");

  testcase(msg, "$(if '\"$FACILITY_NUM\" < \"19\"' alma korte)", "korte");
  testcase(msg, "$(if '\"$FACILITY_NUM\" <= \"19\"' alma korte)", "alma");
  testcase(msg, "$(if '\"$FACILITY_NUM\" == \"19\"' alma korte)", "alma");
  testcase(msg, "$(if '\"$FACILITY_NUM\" != \"19\"' alma korte)", "korte");
  testcase(msg, "$(if '\"$FACILITY_NUM\" > \"19\"' alma korte)", "korte");
  testcase(msg, "$(if '\"$FACILITY_NUM\" >= \"19\"' alma korte)", "alma");
  testcase(msg, "$(if '\"$FACILITY_NUM\" >= \"19\" and \"kicsi\" == \"nagy\"' alma korte)", "korte");
  testcase(msg, "$(if '\"$FACILITY_NUM\" >= \"19\" or \"kicsi\" == \"nagy\"' alma korte)", "alma");

  /* message refs */
  testcase(msg, "$(echo ${HOST}@0 ${PID}@1)", "bzorp 23323");
  testcase(msg, "$(echo $HOST $PID)@0", "bzorp 23323");

  testcase(msg, "$(grep 'facility(local3)' $PID)@0", "23323");
  testcase(msg, "$(grep 'facility(local3)' $PID)@1", "23323");
  testcase(msg, "$(grep 'facility(local3)' $PID)@2", "");

  /* multi-threaded expansion */

  /* name-value pair */
  testcase_multi_thread(msg, "alma $HOST bela", "alma bzorp bela");
  testcase_multi_thread(msg, "kukac $DATE mukac", "kukac Feb 11 10:34:56.000 mukac");
  testcase_multi_thread(msg, "dani $(echo $HOST $DATE $(echo huha)) balint", "dani bzorp Feb 11 10:34:56.000 huha balint");


  /* template syntax errors */
  testcase_failure("${unbalanced_brace", "'}' is missing");
  testcase(msg, "$unbalanced_brace}", "}");
  testcase(msg, "$}", "$}");
  testcase_failure("$(unbalanced_paren", "missing function name or inbalanced '('");
  testcase(msg, "$unbalanced_paren)", ")");


  configuration->version = 0x0201;
  testcase(msg, "$MSGHDR", "syslog-ng[23323]:");
  testcase(msg, "$MSG", "syslog-ng[23323]:árvíztűrőtükörfúrógép");
  testcase(msg, "$MSGONLY", "árvíztűrőtükörfúrógép");
  testcase(msg, "$MESSAGE", "syslog-ng[23323]:árvíztűrőtükörfúrógép");

  log_msg_unref(msg);

  configuration->version = 0x0302;

  msg_str = "syslog-ng: árvíztűrőtükörfúrógép [pid test]";
  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.11.12.13", 1010), &parse_options);

  testcase(msg, "$PID", "");
  log_msg_unref(msg);

  msg_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";

  parse_options.flags = LP_EXPECT_HOSTNAME;
  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.11.12.13", 1010), &parse_options);

  testcase(msg, "$LEGACY_MSGHDR", "");
  testcase(msg, "$MSGHDR", "syslog-ng[23323]: ");
  log_msg_unref(msg);

  msg_str = "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog 3535 ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...";
  parse_options.flags = LP_SYSLOG_PROTOCOL;
  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.11.12.13", 1010), &parse_options);

  testcase(msg, "$PRI", "132");
  testcase(msg, "$HOST", "mymachine");
  testcase(msg, "$PROGRAM", "evntslog");
  testcase(msg, "$PID", "3535");
  testcase(msg, "$MSGID", "ID47");
  testcase(msg, "${.SDATA.exampleSDID@0.iut}", "3");
  testcase(msg, "${.SDATA.exampleSDID@0.eventSource}", "Application");
  testcase(msg, "${.SDATA.exampleSDID@0.eventID}", "1011");
  testcase(msg, "${.SDATA.examplePriority@0.class}", "high");

  /* 12 hour extrema values */
  msg_str = "<155>2006-02-11T00:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";

  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.10.10.10", 1010), &parse_options);

  testcase(msg, "$HOUR12", "12");
  testcase(msg, "$AMPM", "AM");
  log_msg_unref(msg);

  msg_str = "<155>2006-02-11T12:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";

  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.10.10.10", 1010), &parse_options);

  testcase(msg, "$HOUR12", "12");
  testcase(msg, "$AMPM", "PM");
  log_msg_unref(msg);

  msg_str = "<155>323123: 2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";
  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.10.10.10", 1010), &parse_options);
  testcase(msg, "${.SDATA.meta.sequenceId}", "323123");

  log_msg_unref(msg);

  //Very lightweight test for USEC and MSEC
  msg_str = "<155>323123: 2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:árvíztűrőtükörfúrógép";
  msg = log_msg_new(msg_str, strlen(msg_str), g_sockaddr_inet_new("10.10.10.10", 1010), &parse_options);
  testcase(msg, "$S_USEC", "000000");
  testcase(msg, "$S_MSEC", "000");

  log_msg_unref(msg);

  app_shutdown();

  if (success)
    return 0;
  return 1;
}
