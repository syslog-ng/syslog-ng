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
  gchar *template_string = NULL;
  GString  *result = g_string_sized_new(256);
  GError *err = NULL;
  LogMessage *msg = log_msg_new_empty();
  LogTemplate *template =  log_template_new(configuration,NULL);

  log_msg_set_value(msg, LM_V_HOST_FROM, "kismacska", -1);
  msg->timestamps[LM_TS_RECVD].tv_sec = 1139684315;
  msg->timestamps[LM_TS_RECVD].tv_usec = 639000;
  msg->timestamps[LM_TS_RECVD].zone_offset = 0;

  int i = 0;
  while(test_pairs[i].name)
    {
      gchar *name = test_pairs[i].name;
      gchar *value = test_pairs[i].value;
      NVHandle value_handle = log_msg_get_value_handle(name);
      log_msg_set_value(msg,value_handle,value,-1);
      i++;
    }

  template_string = "$(format-welf --key sql.*)";
  assert_true(log_template_compile(template,template_string,&err),"Can't compile valid template");
  log_template_format(template, msg, NULL, 0, 0, "TEST", result);
  assert_string(result->str,"sql.id=127 sql.name=name sql.xyz=\"test data\"","bad formatting");

  template_string = "$(format-welf --key * --exclude=SYSUPTIME --exclude=P_*)";
  assert_true(log_template_compile(template,template_string,&err),"Can't compile valid template");
  log_template_format(template, msg, NULL, 0, 0, "TEST", result);
  assert_string(result->str,
      "AMPM=PM BSDDATE=\"Dec 31 23:59:58\" BSDTAG=0A DATE=\"Dec 31 23:59:58\" DAY=31 FACILITY=kern FACILITY_NUM=0 FILE_FACILITY=0 FILE_LEVEL=0 FULLDATE=\"1969 Dec 31 23:59:58\" HOSTID=00000000 id=123 HOST_FROM=kismacska HOUR=23 HOUR12=11 ISODATE=1969-12-31T23:59:58-00:00 LEVEL=emerg LEVEL_NUM=0 MIN=59 MONTH=12 MONTHNAME=Dec MONTH_ABBREV=Dec MONTH_NAME=December MONTH_WEEK=5 MSEC=000 PRI=0 PRIORITY=emerg R_AMPM=PM R_BSDDATE=\"Feb 11 18:58:35\" R_DATE=\"Feb 11 18:58:35\" R_DAY=11 R_FULLDATE=\"2006 Feb 11 18:58:35\" R_HOUR=18 R_HOUR12=06 R_ISODATE=2006-02-11T18:58:35+00:00 R_MIN=58 R_MONTH=02 R_MONTHNAME=Feb R_MONTH_ABBREV=Feb R_MONTH_NAME=February R_MONTH_WEEK=1 R_MSEC=639 R_SEC=35 R_STAMP=\"Feb 11 18:58:35\" R_TZ=+00:00 R_TZOFFSET=+00:00 R_UNIXTIME=1139684315 R_USEC=639000 R_WEEK=06 R_WEEKDAY=Sat R_WEEK_DAY=7 R_WEEK_DAY_ABBREV=Sat R_WEEK_DAY_NAME=Saturday R_YEAR=2006 R_YEAR_DAY=042 SEC=58 SOURCEIP=127.0.0.1 STAMP=\"Dec 31 23:59:58\" S_AMPM=PM S_BSDDATE=\"Dec 31 23:59:58\" S_DATE=\"Dec 31 23:59:58\" S_DAY=31 S_FULLDATE=\"1969 Dec 31 23:59:58\" S_HOUR=23 S_HOUR12=11 S_ISODATE=1969-12-31T23:59:58-00:00 S_MIN=59 S_MONTH=12 S_MONTHNAME=Dec S_MONTH_ABBREV=Dec S_MONTH_NAME=December S_MONTH_WEEK=5 S_MSEC=000 S_SEC=58 S_STAMP=\"Dec 31 23:59:58\" S_TZ=-00:00 S_TZOFFSET=-00:00 S_UNIXTIME=4294967295 S_USEC=000000 S_WEEK=52 S_WEEKDAY=Wed S_WEEK_DAY=4 S_WEEK_DAY_ABBREV=Wed S_WEEK_DAY_NAME=Wednesday S_YEAR=1969 S_YEAR_DAY=365 TAG=00 TZ=-00:00 TZOFFSET=-00:00 UNIXTIME=4294967295 USEC=000000 WEEK=52 WEEKDAY=Wed WEEK_DAY=4 WEEK_DAY_ABBREV=Wed WEEK_DAY_NAME=Wednesday YEAR=1969 YEAR_DAY=365 something=\"hidden from format\" sql.id=127 sql.name=name sql.xyz=\"test data\""
      ,"bad formatting");

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
  configuration = cfg_new(VERSION_VALUE_3_2);
#ifdef _WIN32
  g_free(module_path);
  module_path = g_strdup("../");
#endif
  assert_true(plugin_load_module("convertfuncs", configuration, NULL),"Can't find convertfuncs plugin in: %s",module_path);
  test_formatwelf();
  return 0;
}
