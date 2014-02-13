#include "testutils.h"
#include "cfg.h"
#include "lib/messages.h"
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "template_lib.h"

#include "dateparse.h"

#define SYSLOGFORMAT_DATEPARSER_TESTCASE(testfunc, ...) { testcase_begin("%s(%s)", #testfunc, #__VA_ARGS__); testfunc(__VA_ARGS__); testcase_end(); }

static void
setup()
{
  msg_init(FALSE);

  configuration = cfg_new(VERSION_VALUE);
  log_msg_registry_init();
  log_template_global_init();
}

static void
tear_down()
{
  log_msg_registry_deinit();
  msg_deinit();
}

LogMessage *
create_empty_sample_message(void)
{
  return log_msg_new_empty();
}

static void
parse_date_format_with_usec(guint32 epoch, gint32 usec, const gchar orig_input[], gint flags, gint32 assume_timezone)
{
  gchar *input = g_strdup(orig_input);
  gchar *input_cpy = input;
  gint input_length = strlen(input);
  time_t sec = epoch;

  LogMessage *msg = create_empty_sample_message();

  gboolean ret = log_msg_parse_date(msg, (const guchar **) &input, &input_length, flags, assume_timezone);

  assert_gboolean_non_fatal(ret, TRUE, "Error in date parsing;");
  assert_gboolean_non_fatal(TRUE, msg->timestamps[LM_TS_STAMP].tv_sec == sec,
                            "Error in date parsing;");
  assert_gboolean_non_fatal(TRUE, msg->timestamps[LM_TS_STAMP].tv_usec == usec,
                            "Error in date parsing;");
  log_msg_unref(msg);
  g_free(input_cpy);
}

static void
parse_date_format(guint32 epoch, const gchar orig_input[], gint flags, gint32 assume_timezone)
{
  parse_date_format_with_usec(epoch, 0, orig_input, flags, assume_timezone);
}

static void
test_parse_legacy_date_format_without_assuming_timezone() {
  parse_date_format(1389179299, "Jan  8 12:08:19", 0, -1);
}

static void
test_parse_legacy_date_format_with_assuming_timezone() {
  parse_date_format(1389179299, "Jan  8 12:08:19", 0, 3600);
}

static void
test_parse_isodate_format()
{
  parse_date_format(1389179299, "2014-01-08T12:08:19+01:00", LP_SYSLOG_PROTOCOL, 3600);
  parse_date_format(1389179299, "2014-01-08T12:08:19+01:00", LP_SYSLOG_PROTOCOL, -1);
  parse_date_format(1389179299, "2014-01-08T12:08:19", LP_SYSLOG_PROTOCOL, 3600);
  parse_date_format(1389179299, "2014-01-08T12:08:19", LP_SYSLOG_PROTOCOL, -1);
}

static void
test_parse_pix_date_format()
{
  parse_date_format(1389179299, "Jan 08 2014 11:08:19 ", 0, 0);
}

static void
test_parse_asa_date_format()
{
  parse_date_format(1389179299, "Jan 08 2014 11:08:19 ", 0, 0);
}

static void
test_parse_rfc3339_date_format()
{
  parse_date_format(1389179299, "2014-01-08T12:08:19+01:00", 0, 0);
}

int main(int argc, char **argv)
{
  setup();

  SYSLOGFORMAT_DATEPARSER_TESTCASE(test_parse_legacy_date_format_without_assuming_timezone);
  SYSLOGFORMAT_DATEPARSER_TESTCASE(test_parse_legacy_date_format_with_assuming_timezone);
  SYSLOGFORMAT_DATEPARSER_TESTCASE(test_parse_isodate_format);
  SYSLOGFORMAT_DATEPARSER_TESTCASE(test_parse_pix_date_format);
  SYSLOGFORMAT_DATEPARSER_TESTCASE(test_parse_asa_date_format);
  SYSLOGFORMAT_DATEPARSER_TESTCASE(test_parse_rfc3339_date_format);

  tear_down();
}
