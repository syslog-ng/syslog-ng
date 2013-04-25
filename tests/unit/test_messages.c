#include "testutils.h"
#include "messages.h"
#include "logmsg.h"

void
test_messages()
{
  LogMessage *msg = NULL;
  gchar *message = NULL;
  msg = msg_event_create(EVT_PRI_ERR, "Test Message", evt_tag_str("name1", "value1"), NULL);
  message = log_msg_get_value(msg, LM_V_MESSAGE, NULL);
  assert_string(message, "Test Message; name1='value1'", "Bad message");
  log_msg_unref(msg);

  msg = msg_event_create(EVT_PRI_ERR, "Test Message", evt_tag_str("name1", "value1"), evt_tag_str("name2", "value2"), NULL);
  message = log_msg_get_value(msg, LM_V_MESSAGE, NULL);
  assert_string(message, "Test Message; name1='value1', name2='value2'", "Bad message");
  log_msg_unref(msg);

  msg = msg_event_create(EVT_PRI_ERR, "Test Message", evt_tag_str("name1", "value1"), evt_tag_str("name2", "value2"), evt_tag_id(125), NULL);
  message = log_msg_get_value(msg, LM_V_MESSAGE, NULL);
  assert_string(message, "Test Message; name1='value1', name2='value2'", "Bad message");
  assert_string(log_msg_get_value(msg, LM_V_MSGID, NULL), "125", "Bad message id");
  log_msg_unref(msg);

  msg = msg_event_create(EVT_PRI_ERR, "Test Message", evt_tag_str("name1", "value1"), evt_tag_id(125), evt_tag_str("name2", "value2"), NULL);
  message = log_msg_get_value(msg, LM_V_MESSAGE, NULL);
  assert_string(message, "Test Message; name1='value1', name2='value2'", "Bad message");
  assert_string(log_msg_get_value(msg, LM_V_MSGID, NULL), "125", "Bad message id");
  log_msg_unref(msg);

  msg = msg_event_create(EVT_PRI_ERR, "Test Message", evt_tag_id(125), evt_tag_str("name1", "value1"), evt_tag_str("name2", "value2"), NULL);
  message = log_msg_get_value(msg, LM_V_MESSAGE, NULL);
  assert_string(message, "Test Message; name1='value1', name2='value2'", "Bad message");
  assert_string(log_msg_get_value(msg, LM_V_MSGID, NULL), "125", "Bad message id");
  log_msg_unref(msg);

  msg = msg_event_create(EVT_PRI_ERR, "Test Message", evt_tag_id(125), NULL);
  message = log_msg_get_value(msg, LM_V_MESSAGE, NULL);
  assert_string(message, "Test Message;", "Bad message");
  assert_string(log_msg_get_value(msg, LM_V_MSGID, NULL), "125", "Bad message id");
  log_msg_unref(msg);

  return;
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  log_msg_registry_init();
  test_messages();
  log_msg_registry_deinit();
  return 0;
}
