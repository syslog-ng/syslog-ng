#include "syslog-ng.h"
#include "logmsg.h"
#include "serialize.h"
#include "apphook.h"
#include "gsockaddr.h"
#include "logpipe.h"
#include "logtransport.h"
#include "logproto.h"
#include "logreader.h"
#include "libtest_memreader.h"
#include "tags.h"
#include "plugin.h"
#include "ack_tracker.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TEST_ASSERT(x, format, value, expected)   \
  do        \
    {         \
        if (!(x))   \
          {     \
            fprintf(stderr, "Testcase failed; msg='%s', cond='%s', value=" format ", expected=" format "\n", message, #x, value, expected); \
            exit(1);    \
          }     \
    }       \
  while (0)

LogMessage *result_msg = NULL;
MsgFormatOptions parse_options;

static void
log_test_msg_ack(LogMessage *msg, AckType ack_type)
{
}

void
log_test_pipe_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  LogSource *self = (LogSource *)s;
  result_msg = log_msg_ref(msg);
  ack_tracker_track_msg(self->ack_tracker, msg);
  msg->ack_func = log_test_msg_ack;
  result_msg = msg;
  return;
}

void
init_option(LogReaderOptions *options)
{
  log_reader_options_defaults(options);
  options->fetch_limit = 100;
  options->follow_freq = 100;
  options->parse_options = parse_options;
  options->super.source_group_tag = log_tags_get_by_name("test_tag");
}

void
log_reader_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
}

int test_case(const gchar *message, const gchar *expected, const gchar *prefix, const gchar *garbage, gboolean send_by_line, GlobalConfig *cfg)
{
  LogReaderOptions reader_options;
  LogTransport *tr = NULL;
  guint32 read_buffer_length = 100000;
  LogReader *reader = NULL;
  const gchar *result = NULL;
  const gchar *line = NULL;
  const gchar *next_line = NULL;
  if (result_msg)
    {
      log_msg_unref(result_msg);
      result_msg = NULL;
    }
  memset(&reader_options, 0, sizeof(LogReaderOptions));
  init_option(&reader_options);
  reader = log_reader_new_memory_source(&reader_options, read_buffer_length, log_reader_notify, log_test_pipe_queue, &tr, prefix, garbage, cfg);
  if(send_by_line)
    {
      next_line = message;
      while(next_line)
        {
          line = memchr(next_line, '\n', strlen(next_line));
          log_test_reader_add_message(tr, next_line, line ? line - next_line + 1 : strlen(next_line));
          log_reader_fetch_log(reader);
          next_line = line + 1;
          if (!line)
            break;
        }
      result =  log_msg_get_value(result_msg, LM_V_MESSAGE, NULL);
      TEST_ASSERT((strcmp(expected, result) == 0), "%s", result, expected);
    }
  else
    {
      log_test_reader_add_message(tr, message, strlen(message));
      log_reader_fetch_log(reader);
      result =  log_msg_get_value(result_msg, LM_V_MESSAGE, NULL);
      TEST_ASSERT((strcmp(expected, result) == 0), "%s", result, expected);
    }
  log_pipe_deinit((LogPipe *)reader);
  log_pipe_unref((LogPipe *)reader);
  return 0;
}

const gchar* msg_without_prefix = "<7>2006-10-29T01:59:59.156+01:00 mymachine.example.com evntslog ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...\n";

const gchar* msg_with_prefix_and_garbage = "<7>2006-10-29T01:59:59.156+01:00 mymachine.example.com evntslog ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] START_MESSAGE BOMAn application event log entry...\nThis is a new line of a multi line message\nThis is an other line of the message\nAnd this is a GARBAGE\n";

const gchar* msg_with_prefix_and_new_prefix = "<7>2006-10-29T01:59:59.156+01:00 mymachine.example.com evntslog ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] START_MESSAGE BOMAn application event log entry...\nThis is a new line of a multi line message\nThis is an other line of the message\n<7>1 2006-10-29T01:59:59.156+01:00 mymachine.example.com evntslog - ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] START_MESSAGE BOMAn application event log entry 2...\n";

const gchar* msg_with_prefix_continue = "This is a new line of the message\nThis is the 3rd line of the message\nAnd this is a GARBAGE\n";

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();
  configuration = cfg_new(VERSION_VALUE_3_2);
  plugin_load_module("syslogformat", configuration, NULL);
  plugin_load_module("basic-proto", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  parse_options.flags &= ~LP_SYSLOG_PROTOCOL;
  msg_format_options_init(&parse_options, configuration);
  test_case(msg_without_prefix, "ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...", NULL, NULL, FALSE, configuration);
  test_case(msg_without_prefix, "ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...", NULL, NULL, TRUE, configuration);

  test_case(msg_with_prefix_and_garbage,"ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] START_MESSAGE BOMAn application event log entry...\nThis is a new line of a multi line message\nThis is an other line of the message\nAnd this is a ", ".*START_MESSAGE.*", "GARBAGE", FALSE, configuration);
  test_case(msg_with_prefix_and_garbage,"ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] START_MESSAGE BOMAn application event log entry...\nThis is a new line of a multi line message\nThis is an other line of the message\nAnd this is a ", ".*START_MESSAGE.*", "GARBAGE", TRUE, configuration);

  test_case(msg_with_prefix_and_garbage,"ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] START_MESSAGE BOMAn application event log entry...\nThis is a new line of a multi line message\nThis is an other line of the message\nAnd this is a ", ".*START_MESSAGE.*", "GARBAGE", FALSE, configuration);
  test_case(msg_with_prefix_and_garbage,"ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] START_MESSAGE BOMAn application event log entry...\nThis is a new line of a multi line message\nThis is an other line of the message\nAnd this is a ", ".*START_MESSAGE.*", "GARBAGE", TRUE, configuration);

  test_case(msg_with_prefix_and_new_prefix, "ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] START_MESSAGE BOMAn application event log entry...\nThis is a new line of a multi line message\nThis is an other line of the message",".*START_MESSAGE.*", NULL, FALSE, configuration);
  test_case(msg_with_prefix_and_new_prefix, "ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] START_MESSAGE BOMAn application event log entry...\nThis is a new line of a multi line message\nThis is an other line of the message",".*START_MESSAGE.*", NULL, TRUE, configuration);

  test_case(msg_with_prefix_continue, "is a new line of the message\nThis is the 3rd line of the message", ".*START_MESSAGE.*", ".*GARBAGE.*", FALSE, configuration);
  test_case(msg_with_prefix_continue, "is a new line of the message\nThis is the 3rd line of the message", ".*START_MESSAGE.*", ".*GARBAGE.*", TRUE, configuration);
  app_shutdown();
  return 0;
}
