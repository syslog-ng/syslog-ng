#include "testutils.h"
#include "control/control-server.h"
#include "control/control.c"
#include "stats/stats-cluster.h"
#include "stats/stats-registry.h"
#include "apphook.h"

void
test_log()
{
  GString *command = g_string_sized_new(128);
  GString *reply;

  g_string_assign(command,"LOG");
  reply = control_connection_message_log(command);
  assert_string(reply->str, "Invalid arguments received, expected at least one argument", "Bad reply");
  g_string_free(reply, TRUE);

  g_string_assign(command,"LOG fakelog");
  reply = control_connection_message_log(command);
  assert_string(reply->str, "Invalid arguments received", "Bad reply");
  g_string_free(reply, TRUE);

  verbose_flag = 0;
  debug_flag = 1;
  trace_flag = 1;
  g_string_assign(command,"LOG VERBOSE");
  reply = control_connection_message_log(command);
  assert_string(reply->str, "VERBOSE=0", "Bad reply");
  g_string_free(reply, TRUE);

  g_string_assign(command,"LOG VERBOSE ON");
  reply = control_connection_message_log(command);
  assert_string(reply->str, "OK", "Bad reply");
  assert_gint(verbose_flag,1,"Flag isn't changed");
  g_string_free(reply, TRUE);

  g_string_assign(command,"LOG VERBOSE OFF");
  reply = control_connection_message_log(command);
  assert_string(reply->str, "OK", "Bad reply");
  assert_gint(verbose_flag,0,"Flag isn't changed");
  g_string_free(reply, TRUE);

  debug_flag = 0;
  verbose_flag = 1;
  trace_flag = 1;
  g_string_assign(command,"LOG DEBUG");
  reply = control_connection_message_log(command);
  assert_string(reply->str, "DEBUG=0", "Bad reply");
  g_string_free(reply, TRUE);

  trace_flag = 0;
  verbose_flag = 1;
  debug_flag = 1;
  g_string_assign(command,"LOG TRACE");
  reply = control_connection_message_log(command);
  assert_string(reply->str, "TRACE=0", "Bad reply");
  g_string_free(reply, TRUE);

  g_string_free(command, TRUE);
  return;
}

void
test_stats()
{
  GString *reply = NULL;
  GString *command = g_string_sized_new(128);
  StatsCounterItem *counter = NULL;
  gchar **stats_result;

  stats_lock();
  stats_register_counter(0, SCS_CENTER, "id", "received", SC_TYPE_PROCESSED, &counter);
  stats_unlock();

  g_string_assign(command,"STATS");

  reply = control_connection_send_stats(command);
  stats_result = g_strsplit(reply->str, "\n", 2);
  assert_string(stats_result[0], "SourceName;SourceId;SourceInstance;State;Type;Number",
                "Bad reply");
  g_strfreev(stats_result);
  g_string_free(reply, TRUE);

  g_string_free(command, TRUE);
  return;
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();
  test_log();
  test_stats();
  app_shutdown();
  return 0;
}
