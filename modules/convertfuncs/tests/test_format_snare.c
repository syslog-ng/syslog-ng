#include "logmsg.h"
#include "cfg.h"
#include "plugin.h"
#include "testutils.h"
#include <stdio.h>

gchar *event_pairs[] = {"EVENT_NAME","Application",
                        "EVENT_GLOBAL_COUNTER","27",
                        "WEEKDAY","Fri",
                        "MONTHNAME","Sep",
                        "DAY","17",
                        "HOUR","10",
                        "MIN","16",
                        "SEC","40",
                        "YEAR","2010",
                        "EVENT_ID","1000",
                        "EVENT_PROVIDER","application",
                        "EVENT_SNARE_USER","Unknown User",
                        "EVENT_SID_TYPE","N/A",
                        "EVENT_TYPE","Warning",
                        "EVENT_HOST","ZTS-WIN014",
                        "EVENT_CATEGORY","None",
                        "EVENT_MESSAGE","test message from windows#XP",
                        "EVENT_CONTAINER_COUNTER","3",
                        NULL,NULL};

gchar *file_pairs[] = {"FILE_NAME","test.txt",
                       "FILE_SIZE","127",
                       "FILE_POS","0",
                       "FILE_MESSAGE","test message from windows XP",
                       NULL,NULL};

void
test_snare_template_function()
{
  LogMessage *msg;
  LogTemplate *template = log_template_new(configuration,NULL);
  gchar *template_string;
  GString *result = g_string_sized_new(512);
  GError *err = NULL;
  LogStamp stamp;
  /* May 21 11:22:12 */
  stamp.tv_sec = 1337592132;
  stamp.tv_usec = 717481;
  stamp.zone_offset = 7200;
  gchar *expected = NULL;

  gint i = 0;

  msg = log_msg_new_empty();
  msg->rcptid = 27;

  while(event_pairs[i])
    {
      gchar *handle_name = event_pairs[i++];
      NVHandle handle_value = log_msg_get_value_handle(handle_name);
      gchar *value = event_pairs[i++];
      log_msg_set_value(msg,handle_value,value,-1);
    }

  msg->timestamps[1] = stamp;
  msg->timestamps[0] = msg->timestamps[1];

  expected = "<0>May 21 11:22:12  MSWinEventLog\t1\tApplication\t27\tMon May 21 11:22:12 2012\t1000\tapplication\tUnknown User\tN/A\tWarning\tZTS-WIN014\tNone\t\ttest message from windows#XP\t3\n";
  template_string = "$(format-snare)";
  assert_true(log_template_compile(template,template_string,&err),"CAN't COMPILE!");
  log_template_format(template, msg, NULL, 0, 0, "TEST", result);
  assert_string(result->str,expected,"Bad formatting");

  expected = "<0>May 21 11:22:12  MSWinEventLog#1#Application#27#Mon May 21 11:22:12 2012#1000#application#Unknown User#N/A#Warning#ZTS-WIN014#None##test message from windows XP#3\n";
  template_string = "$(format-snare -d #)";
  assert_true(log_template_compile(template,template_string,&err),"CAN't COMPILE!");
  log_template_format(template, msg, NULL, 0, 0, "TEST", result);
  assert_string(result->str,expected,"Bad delimiter handling");

  expected = "<0>May 21 11:22:12  MSWinEventLog\t2\tApplication\t27\tMon May 21 11:22:12 2012\t1000\tapplication\tUnknown User\tN/A\tWarning\tZTS-WIN014\tNone\t\ttest message from windows#XP\t3\n";
  template_string = "$(format-snare -c 2)";
  assert_true(log_template_compile(template,template_string,&err),"CAN't COMPILE!");
  log_template_format(template, msg, NULL, 0, 0, "TEST", result);
  assert_string(result->str,expected,"Bad criticality handling");

  log_template_unref(template);
  log_msg_unref(msg);

  msg = log_msg_new_empty();
  template = log_template_new(configuration,NULL);
  i = 0;
  while(file_pairs[i])
    {
      gchar *handle_name = file_pairs[i++];
      NVHandle handle_value = log_msg_get_value_handle(handle_name);
      gchar *value = file_pairs[i++];
      log_msg_set_value(msg,handle_value,value,-1);
    }

  msg->timestamps[1] = stamp;
  msg->timestamps[0] = msg->timestamps[1];
  log_msg_set_value(msg,LM_V_MESSAGE,"test.txt: test message form windows XP",-1);

  template_string = "$(format-snare)";
  expected = "<0>May 21 11:22:12  test.txt: test message form windows XP\n";
  assert_true(log_template_compile(template,template_string,&err),"CAN't COMPILE!");
  log_template_format(template, msg, NULL, 0, 0, "TEST", result);
  assert_string(result->str,expected,"Bad formatting");

  template_string = "$(format-snare -a 1)";
  assert_false(log_template_compile(template,template_string,&err),"Invalid template compiled %s",template_string);
  g_clear_error(&err);

  log_template_unref(template);
  log_msg_unref(msg);

  msg = log_msg_new_empty();
  template = log_template_new(configuration,NULL);
  msg->timestamps[1] = stamp;
  msg->timestamps[0] = msg->timestamps[1];
  log_msg_set_value(msg,LM_V_MESSAGE,"---MARK---",-1);
  log_msg_set_value(msg,LM_V_HOST, "testhost", -1);

  template_string = "$(format-snare)";
  expected = "<0>May 21 11:22:12 testhost ---MARK---\n";
  assert_true(log_template_compile(template,template_string,&err),"CAN't COMPILE!");
  log_template_format(template, msg, NULL, 0, 0, "TEST", result);
  assert_string(result->str,expected,"Bad formatting");

  log_template_unref(template);
  log_msg_unref(msg);
  g_string_free(result,TRUE);
}


int main(void)
{
  log_template_global_init();
  log_msg_registry_init();
  configuration = cfg_new(0x0302);
#ifdef _WIN32
  g_free(module_path);
  module_path = g_strdup("../");
#endif
  assert_true(plugin_load_module("convertfuncs", configuration, NULL),"Can't find convertfuncs plugin in: %s",module_path);
  test_snare_template_function();
  return 0;
}
