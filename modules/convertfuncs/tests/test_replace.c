#include "logmsg.h"
#include "cfg.h"
#include "plugin.h"
#include "testutils.h"
#include <stdio.h>

#define ERROR_STRING "replace: syntax error. Syntax is: replace subject search_character replace_character"

gchar *test_cases[] = {"$(replace $MESSAGE #)","this is a test message",ERROR_STRING,
                      "$(replace $MESSAGE aaa b)","this is a test message",ERROR_STRING,
                      "$(replace $MESSAGE c bbb)","this is a test message",ERROR_STRING,
                      "$(replace $MESSAGE a b)","this is a test message","this is b test messbge",
                      NULL,NULL,NULL};

void
test_replace_template_function()
{
  GError *err = NULL;
  gchar *template_string;
  gchar *expected;
  gchar *message;
  GString *result = g_string_sized_new(256);
  LogTemplate *template = log_template_new(configuration,NULL);
  LogMessage *msg;
  gint i = 0;
  while(test_cases[i])
    {
      template_string = test_cases[i++];
      message = test_cases[i++];
      expected = test_cases[i++];
      assert_true(log_template_compile(template,template_string,&err),"CAN't COMPILE!");

      msg = log_msg_new_empty();
      log_msg_set_value(msg,LM_V_MESSAGE,message,-1);
      log_template_format(template, msg, NULL, 0, 0, "TEST", result);

      assert_string(result->str,expected,"BAD RESULT!");

      log_msg_unref(msg);
    }
  g_string_free(result,TRUE);
  log_template_unref(template);
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
  plugin_load_module("convertfuncs", configuration, NULL);
  test_replace_template_function();
  return 0;
}
