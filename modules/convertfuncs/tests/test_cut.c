#include "logmsg.h"
#include "cfg.h"
#include "plugin.h"
#include "testutils.h"
#include <stdio.h>

void test_cut_template_function_invalid_syntax()
{
  gchar *invalid_syntax[] = { "$(cut $HOST)", /* without any parameters */
                              "$(cut)", /* without any parameters */
                              "$(cut --asdasd=asdasd --delimiters=.; --start_from=1 --length=0 $HOST)", /* with unknown option name */
                              "$(cut --delimiters=.; --start_from=1 --length=0)", /* subject missing */
                              "$(cut --start_from=1 --length=0 $HOST)",           /* delimiters missing */
                              "$(cut --delimiters=.; --length=0 $HOST)",          /* start_from missing */
                              "$(cut --delimiters=.; --start_from=1 $HOST)",      /* length missing */
                              "$(cut --delimiters=.; --start_from=65535 --length=0 $HOST)", /*invalid start_from */
                              "$(cut --delimiters=.; --start_from=-65535 --length=0 $HOST)", /*invalid start_from */
                              "$(cut --delimiters=.; --start_from=256 --length=0 $HOST)", /*invalid start_from */
                              "$(cut --delimiters=.; --start_from=-256 --length=0 $HOST)", /*invalid start_from */
                              "$(cut --delimiters=.; --start_from=0 --length=-2 $HOST)",    /*invalid length */
                              "$(cut --delimiters=.; --start_from=0 --length=-1 $HOST)",    /*invalid length */
                              "$(cut --delimiters=.; --start_from=0 --length=256 $HOST)",    /*invalid length */
                              "$(cut --delimiters=.; --start_from=0 --length=asb $HOST)",    /*invalid length */
                              NULL};

  int i = 0;
  LogTemplate *template = log_template_new(configuration,NULL);
  while(invalid_syntax[i])
    {
      GError *err = NULL;
      assert_false(log_template_compile(template,invalid_syntax[i],&err),"Compile invalid template %s",invalid_syntax[i]);
      g_clear_error(&err);
      i++;
    }
  log_template_unref(template);
}

typedef struct _test_case {
  gchar *template_string;
  gchar *expected_output;
} test_case;

void test_cut_template_function()
{
  LogTemplate *template = log_template_new(configuration,NULL);
  LogMessage *msg = log_msg_new_empty();
  gboolean tc_result = TRUE;
  GString *result = g_string_sized_new(512);
  test_case test_cases[] = {
          {"$(cut --delimiters=. --start_from=0 --length=0 a.b.c.d)","a.b.c.d"},
          {"$(cut --delimiters=. --start_from=1 --length=0 a.b.c.d)","b.c.d"},
          {"$(cut --delimiters=. --start_from=2 --length=0 a.b.c.d)","c.d"},
          {"$(cut --delimiters=. --start_from=3 --length=0 a.b.c.d)","d"},
          {"$(cut --delimiters=. --start_from=4 --length=0 a.b.c.d)","d"},
          {"$(cut --delimiters=. --start_from=5 --length=0 a.b.c.d)","d"},
          {"$(cut --delimiters=. --start_from=255 --length=0 a.b.c.d)","d"},
          {"$(cut --delimiters=1 --start_from=0 --length=1 a.b1c.d)","a.b"},

          {"$(cut --delimiters=. --start_from=-1 --length=0 a.b.c.d)","d"},
          {"$(cut --delimiters=. --start_from=-2 --length=0 a.b.c.d)","c.d"},
          {"$(cut --delimiters=. --start_from=-3 --length=0 a.b.c.d)","b.c.d"},
          {"$(cut --delimiters=. --start_from=-4 --length=0 a.b.c.d)","a.b.c.d"},
          {"$(cut --delimiters=. --start_from=-5 --length=0 a.b.c.d)","a.b.c.d"},
          {"$(cut --delimiters=. --start_from=-255 --length=0 a.b.c.d)","a.b.c.d"},


          {"$(cut --delimiters=. --start_from=0 --length=1 a.b.c.d)","a"},
          {"$(cut --delimiters=. --start_from=0 --length=2 a.b.c.d)","a.b"},
          {"$(cut --delimiters=. --start_from=0 --length=3 a.b.c.d)","a.b.c"},
          {"$(cut --delimiters=. --start_from=0 --length=4 a.b.c.d)","a.b.c.d"},
          {"$(cut --delimiters=. --start_from=0 --length=5 a.b.c.d)","a.b.c.d"},
          {"$(cut --delimiters=. --start_from=0 --length=255 a.b.c.d)","a.b.c.d"},

          {"$(cut --delimiters=.;zn --start_from=0 --length=1 a.b;czdne)","a"},
          {"$(cut --delimiters=.;zn --start_from=0 --length=2 a.b;czdne)","a.b"},
          {"$(cut --delimiters=.;zn --start_from=0 --length=3 a.b;czdne)","a.b;c"},
          {"$(cut --delimiters=.;zn --start_from=0 --length=4 a.b;czdne)","a.b;czd"},
          {"$(cut --delimiters=.;zn --start_from=0 --length=5 a.b;czdne)","a.b;czdne"},
          {"$(cut --delimiters=.;zn --start_from=0 --length=255 a.b;czdne)","a.b;czdne"},

          {"$(cut --delimiters=.;zn --start_from=1 --length=1 a.b;czdne)","b"},
          {"$(cut --delimiters=.;zn --start_from=-2 --length=2 \"a.b;czd ne\")","d ne"},
          {"$(cut --delimiters=.;zn --start_from=-3 --length=5 \"a.b;czd ne\")","czd ne"},
          {"$(cut --delimiters=.;zn --start_from=-4 --length=3 \"a.b;czd ne\")","b;czd "},
          {"$(cut --delimiters=.;zn --start_from=-3 --length=3 \"a.b;czd ne\")","czd ne"},

          {"$(cut --delimiters=@ --start_from=1 --length=1 a.b;czdne)","a.b;czdne"},
          {"$(cut --delimiters=@ --start_from=1 --length=1 a@@.b;czdne)",""},
          {"$(cut --delimiters=@ --start_from=0 --length=1 somebody@balabit.hu)","somebody"},
          {"$(cut --delimiters=@. --start_from=1 --length=3 somebody@balabit.hu)","balabit.hu"},
          {"$(cut --delimiters=. --start_from=0 --length=1 $(cut --delimiters=@ --start_from=1 --length=0 somebody.xyz@balabit.hu))","balabit"},
          {NULL},
       };
  int i = 0;
  while(test_cases[i].template_string)
    {
      GError *err = NULL;
      assert_true(log_template_compile(template,test_cases[i].template_string,&err),"Can't compile valid template %s",test_cases[i].template_string);
      log_template_format(template, msg, NULL, 0, 0, "TEST", result);
      if (!assert_nstring_non_fatal(result->str,result->len,test_cases[i].expected_output,-1,"Bad formatting template: %s",test_cases[i].template_string))
        {
          tc_result = FALSE;
        }
      i++;
    }

  log_template_unref(template);
  log_msg_unref(msg);
  g_string_free(result,TRUE);
  assert_true(tc_result,"Some assertion failed");
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
  test_cut_template_function_invalid_syntax();
  test_cut_template_function();
  return 0;
}
