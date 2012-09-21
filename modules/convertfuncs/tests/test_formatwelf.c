#include "logmsg.h"
#include "cfg.h"
#include "plugin.h"
#include "testutils.h"
#include <stdio.h>

typedef struct _nv_pair {
  gchar *name;
  gchar *value;
} nv_pair;

void
test_formatwelf()
{
  nv_pair test_pairs[] = {
                          {"sql.name","name"},
                          {"sql.id","127"},
                          {"sql.xyz","test data"},
                          {"something","hidden from format"},
                          {"id","123"},
                          {NULL, NULL}
                         };
  gchar *template_string = "$(format-welf --key sql.*)";
  GString  *result = g_string_sized_new(256);
  GError *err = NULL;
  LogMessage *msg = log_msg_new_empty();
  LogTemplate *template =  log_template_new(configuration,NULL);

  int i = 0;
  while(test_pairs[i].name)
    {
      gchar *name = test_pairs[i].name;
      gchar *value = test_pairs[i].value;
      NVHandle value_handle = log_msg_get_value_handle(name);
      log_msg_set_value(msg,value_handle,value,-1);
      i++;
    }

  assert_true(log_template_compile(template,template_string,&err),"Can't compile valid template");
  log_template_format(template, msg, NULL, 0, 0, "TEST", result);
  assert_string(result->str,"sql.id=127 sql.name=name sql.xyz=\"test data\"","bad formatting");

  template_string = "$(format-welf --key *)";
  assert_true(log_template_compile(template,template_string,&err),"Can't compile valid template");
  log_template_format(template, msg, NULL, 0, 0, "TEST", result);
  assert_string(result->str,"id=123 something=\"hidden from format\" sql.id=127 sql.name=name sql.xyz=\"test data\"","bad formatting");

  template_string = "$(format-welf --invalid-args hello --key sql.*)";
  assert_false(log_template_compile(template,template_string,&err),"compile invalid template");


  g_string_free(result,TRUE);
  log_msg_unref(msg);
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
  test_formatwelf();
  return 0;
}
