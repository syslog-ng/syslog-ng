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

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SDATA_FILE_PREFIX ".SDATA.file@18372.4."
#define SDATA_FILE_NAME SDATA_FILE_PREFIX "name"
#define SDATA_FILE_SIZE SDATA_FILE_PREFIX "size"
#define SDATA_FILE_POS SDATA_FILE_PREFIX "position"

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

void log_test_msg_ack(LogMessage *msg, gpointer user_data, gboolean need_pos_tracking)
{

}

void
log_test_pipe_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options, gpointer user_data)
{
  result_msg = log_msg_ref(msg);
  msg->ack_func = log_test_msg_ack;
  msg->ack_userdata = s;
  return;
}

void
log_reader_notify(LogPipe *s, LogPipe *sender, gint notify_code, gpointer user_data)
{
}

void
init_option(LogReaderOptions *options)
{
  log_reader_options_defaults(options);
  options->fetch_limit = 100;
  options->super.source_group_tag = log_tags_get_by_name("test_tag");
}

int test_case(const gchar *message, const gchar *expected_sdata, const gchar *expected_filename, guint64 expected_pos, guint64 expected_size, GlobalConfig *cfg)
{
  LogReaderOptions reader_options;
  guint32 read_buffer_length = 100000;
  LogReader *reader = NULL;
  LogTransport *tr = NULL;
  GString *res = g_string_new("");
  NVHandle handle;
  const gchar *fname;
  const gchar *size_str;
  const gchar *pos_str;
  guint64 pos = 0;
  guint64 size = 0;
  if (result_msg)
    {
      log_msg_unref(result_msg);
      result_msg = NULL;
    }
  memset(&reader_options, 0, sizeof(LogReaderOptions));
  init_option(&reader_options);
  reader_options.follow_freq = 100;
  reader_options.parse_options = parse_options;
  reader = log_reader_new_file_source(&reader_options, read_buffer_length, log_reader_notify, log_test_pipe_queue, &tr, cfg);
  log_reader_set_follow_filename((LogPipe *)reader, "test_filereader.txt");
  log_test_reader_add_message(tr, message, strlen(message));
  log_reader_fetch_log(reader);
  sleep(1);
  log_msg_format_sdata(result_msg, res, 0);
  TEST_ASSERT((strcmp(expected_sdata, res->str) == 0), "%s", res->str, expected_sdata);

  handle = log_msg_get_value_handle(SDATA_FILE_NAME);
  fname = log_msg_get_value(result_msg,handle,NULL);
  TEST_ASSERT((strcmp(expected_filename, fname) == 0), "%s", fname, expected_filename);

  handle = log_msg_get_value_handle(SDATA_FILE_POS);
  pos_str = log_msg_get_value(result_msg,handle,NULL);
  pos = atoi(pos_str);
  TEST_ASSERT(pos == expected_pos, "%" G_GUINT64_FORMAT, pos, expected_pos);

  handle = log_msg_get_value_handle(SDATA_FILE_SIZE);
  size_str = log_msg_get_value(result_msg,handle,NULL);
  size = atoi(size_str);
  TEST_ASSERT(size == expected_size, "%" G_GUINT64_FORMAT, size, expected_size);
  return 0;
}

const gchar* test_msg = "<7>2006-10-29T01:59:59.156+01:00 mymachine.example.com evntslog ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...\n";

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();
  //Testing syslog protocol file@ SD-ID generating if file source is used
  configuration = cfg_new(VERSION_VALUE_3_2);
  plugin_load_module("syslogformat", configuration, NULL);
  plugin_load_module("basic-proto", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
  test_case(test_msg,"[timeQuality isSynced=\"0\" tzKnown=\"1\"][file@18372.4 position=\"203\" size=\"1\" name=\"test_filereader.txt\"]","test_filereader.txt", strlen(test_msg),1, configuration);
  app_shutdown();
  return 0;
}
