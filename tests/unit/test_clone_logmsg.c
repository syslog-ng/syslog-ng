#include "testutils.h"
#include "msg_parse_lib.h"
#include "syslog-ng.h"
#include "logmsg.h"
#include "serialize.h"
#include "apphook.h"
#include "gsockaddr.h"
#include "logpipe.h"
#include "cfg.h"
#include "plugin.h"

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void
assert_log_message_has_tag(LogMessage *log_message, const gchar *tag_name)
{
  assert_true(log_msg_is_tag_by_name(log_message, tag_name), "Message has no '%s' tag", tag_name);
}

void
assert_log_message_value(LogMessage *self, NVHandle handle, const gchar *expected_value)
{
  gssize key_name_length;
  const gchar *key_name = log_msg_get_value_name(handle, &key_name_length);
  const gchar *actual_value = log_msg_get_value(self, handle, NULL);

  assert_string(actual_value, expected_value, "Value is not expected for key %s", key_name);
}

void
assert_new_log_message_attributes(LogMessage *log_message)
{
  assert_log_message_value(log_message, LM_V_HOST, "newhost");
  assert_log_message_value(log_message, LM_V_HOST_FROM, "newhost");
  assert_log_message_value(log_message, LM_V_MESSAGE, "newmsg");
  assert_log_message_value(log_message, LM_V_PROGRAM, "newprogram");
  assert_log_message_value(log_message, LM_V_PID, "newpid");
  assert_log_message_value(log_message, LM_V_MSGID, "newmsgid");
  assert_log_message_value(log_message, LM_V_SOURCE, "newsource");
  assert_log_message_value(log_message, log_msg_get_value_handle("newvalue"), "newvalue");
}

void
set_new_log_message_attributes(LogMessage *log_message)
{
  log_msg_set_value(log_message, LM_V_HOST, "newhost", -1);
  log_msg_set_value(log_message, LM_V_HOST_FROM, "newhost", -1);
  log_msg_set_value(log_message, LM_V_MESSAGE, "newmsg", -1);
  log_msg_set_value(log_message, LM_V_PROGRAM, "newprogram", -1);
  log_msg_set_value(log_message, LM_V_PID, "newpid", -1);
  log_msg_set_value(log_message, LM_V_MSGID, "newmsgid", -1);
  log_msg_set_value(log_message, LM_V_SOURCE, "newsource", -1);
  log_msg_set_value(log_message, log_msg_get_value_handle("newvalue"), "newvalue", -1);
}

void
assert_log_messages_saddr(LogMessage *log_message_a, LogMessage *log_message_b)
{
  gchar address_a[256], address_b[256];

  g_sockaddr_format(log_message_a->saddr, address_a, sizeof(address_a), GSA_FULL);
  g_sockaddr_format(log_message_b->saddr, address_b, sizeof(address_b), GSA_FULL);
  assert_string(address_a, address_b, "Socket address is not expected");
}

void
assert_structured_data_of_messages(LogMessage *log_message_a, LogMessage *log_message_b)
{
  GString *structured_data_string_a = g_string_sized_new(0);
  GString *structured_data_string_b = g_string_sized_new(0);

  log_msg_format_sdata(log_message_a, structured_data_string_a, 0);
  log_msg_format_sdata(log_message_b, structured_data_string_b, 0);
  assert_string(structured_data_string_a->str, structured_data_string_b->str, "Structure data string are not the same");

  g_string_free(structured_data_string_a, TRUE);
  g_string_free(structured_data_string_b, TRUE);
}

void
assert_log_message_values_equal(LogMessage *log_message_a, LogMessage *log_message_b, NVHandle handle)
{
  gssize key_name_length;
  const gchar *key_name = log_msg_get_value_name(handle, &key_name_length);
  const gchar *value_a = log_msg_get_value(log_message_a, handle, NULL);
  const gchar *value_b = log_msg_get_value(log_message_b, handle, NULL);

  assert_string(value_a, value_b, "Value is not expected for key %s", key_name);
}

void
assert_log_messages_equal(LogMessage *log_message_a, LogMessage *log_message_b)
{
  assert_guint16(log_message_a->pri, log_message_b->pri, "Priorities are not the same");

  assert_gint(log_message_a->timestamps[LM_TS_STAMP].tv_sec, log_message_b->timestamps[LM_TS_STAMP].tv_sec,
      "Timestamps are not the same");
  assert_guint32(log_message_a->timestamps[LM_TS_STAMP].tv_usec, log_message_b->timestamps[LM_TS_STAMP].tv_usec,
      "Timestamps usec are not the same");
  assert_guint32(log_message_a->timestamps[LM_TS_STAMP].zone_offset, log_message_b->timestamps[LM_TS_STAMP].zone_offset,
      "Timestamp offset are not the same");

  assert_log_message_values_equal(log_message_a, log_message_b, LM_V_HOST);
  assert_log_message_values_equal(log_message_a, log_message_b, LM_V_PROGRAM);
  assert_log_message_values_equal(log_message_a, log_message_b, LM_V_MESSAGE);
  assert_log_message_values_equal(log_message_a, log_message_b, LM_V_PID);
  assert_log_message_values_equal(log_message_a, log_message_b, LM_V_MSGID);

  assert_structured_data_of_messages(log_message_a, log_message_b);
  assert_log_messages_saddr(log_message_a, log_message_b);
}

void
test_cloning_with_log_message(gchar *msg)
{
  LogMessage *original_log_message, *log_message, *cloned_log_message;
  regex_t bad_hostname;
  GSockAddr *addr = g_sockaddr_inet_new("10.10.10.10", 1010);
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  parse_options.flags = LP_SYSLOG_PROTOCOL;
  parse_options.bad_hostname = &bad_hostname;

  original_log_message = log_msg_new(msg, strlen(msg), addr, &parse_options);
  log_message = log_msg_new(msg, strlen(msg), addr, &parse_options);

  log_msg_set_tag_by_name(log_message, "newtag");
  path_options.ack_needed = FALSE;

  cloned_log_message = log_msg_clone_cow(log_message, &path_options);
  assert_log_messages_equal(cloned_log_message, original_log_message);

  set_new_log_message_attributes(cloned_log_message);

  assert_log_messages_equal(log_message, original_log_message);
  assert_new_log_message_attributes(cloned_log_message);
  assert_log_message_has_tag(cloned_log_message, "newtag");

  log_msg_unref(cloned_log_message);
  log_msg_unref(log_message);
  log_msg_unref(original_log_message);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();

  init_and_load_syslogformat_module();

  test_cloning_with_log_message(
      "<7>1 2006-10-29T01:59:59.156+01:00 mymachine.example.com evntslog - ID47 [exampleSDID@0 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...");
  test_cloning_with_log_message(
      "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [exampleSDID@0 iut=\"3\"] [eventSource=\"Application\" eventID=\"1011\"][examplePriority@0 class=\"high\"] BOMAn application event log entry...");

  deinit_syslogformat_module();
  app_shutdown();
  return 0;
}
