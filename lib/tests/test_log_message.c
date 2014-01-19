#include "msg_parse_lib.h"
#include "apphook.h"
#include "logpipe.h"

#include <stdlib.h>

LogMessage *
construct_log_message(void)
{
  const gchar *raw_msg = "foo";
  LogMessage *msg;

  msg = log_msg_new(raw_msg, strlen(raw_msg), NULL, &parse_options);
  log_msg_set_value(msg, LM_V_HOST, raw_msg, -1);
  return msg;
}

LogMessage *
clone_cow_log_message(LogMessage *msg)
{
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  return log_msg_clone_cow(msg, &path_options);
}

void
test_log_message_can_be_created_and_freed(void)
{
  LogMessage *msg = construct_log_message();
  log_msg_unref(msg);
}

NVHandle nv_handle;
NVHandle sd_handle;
const gchar *tag_name = "tag";

LogMessage *
construct_log_message_with_all_bells_and_whistles(void)
{
  LogMessage *msg = construct_log_message();

  nv_handle = log_msg_get_value_handle("foo");
  sd_handle = log_msg_get_value_handle(".SDATA.foo.bar");

  log_msg_set_value(msg, nv_handle, "value", -1);
  log_msg_set_value(msg, sd_handle, "value", -1);
  msg->saddr = g_sockaddr_inet_new("1.2.3.4", 5050);
  log_msg_set_tag_by_name(msg, tag_name);
  return msg;
}

void
assert_log_msg_clear_clears_all_properties(LogMessage *msg)
{
  log_msg_clear(msg);

  assert_string("", log_msg_get_value(msg, nv_handle, NULL), "Message still contains value after log_msg_clear");
  assert_string("", log_msg_get_value(msg, sd_handle, NULL), "Message still contains sdata value after log_msg_clear");
  assert_true(msg->saddr == NULL, "Message still contains an saddr after log_msg_clear");
  assert_false(log_msg_is_tag_by_name(msg, tag_name), "Message still contains a valid tag after log_msg_clear");
}

void
test_log_message_can_be_cleared(void)
{
  LogMessage *msg, *clone;

  msg = construct_log_message_with_all_bells_and_whistles();
  clone = clone_cow_log_message(msg);

  assert_log_msg_clear_clears_all_properties(clone);
  log_msg_unref(clone);

  assert_log_msg_clear_clears_all_properties(msg);
  log_msg_unref(msg);
}

void
test_log_message(void)
{
  MSG_TESTCASE(test_log_message_can_be_created_and_freed);
  MSG_TESTCASE(test_log_message_can_be_cleared);
}

void
test_log_msg_get_value_with_time_related_macro(void)
{
  LogMessage *msg;
  gssize value_len;
  NVHandle handle;
  const char *date_value;

  msg = log_msg_new_empty();
  msg->timestamps[LM_TS_STAMP].tv_sec = 1389783444;

  handle = log_msg_get_value_handle("ISODATE");
  date_value = log_msg_get_value(msg, handle, &value_len);
  assert_string(date_value, "2014-01-15T10:57:23-00:00", "ISODATE macro value does not match!");

  log_msg_unref(msg);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();

  init_and_load_syslogformat_module();

  test_log_message();
  test_log_msg_get_value_with_time_related_macro();

  app_shutdown();
  return 0;
}
