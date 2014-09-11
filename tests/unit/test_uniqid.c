#include "testutils.h"
#include "syslog-ng.h"
#include "logmsg.h"
#include "template/templates.h"
#include "apphook.h"
#include <stdio.h>
#include <string.h>

#define UNIQID_TESTCASE(testfunc, ...)  { testcase_begin("%s(%s)", #testfunc, #__VA_ARGS__); testfunc(__VA_ARGS__); testcase_end(); }

static void
test_uniqid_template_format()
{
  LogMessage *msg = log_msg_new_mark();
  msg->hostid = 0xcafebabe;
  msg->rcptid = 0xafc;
  const gchar *expected="cafebabe@0000000000000afc";
  gssize len = 0;
  const gchar *value = log_msg_get_macro_value(msg, log_macro_lookup("UNIQID", strlen("UNIQID")+1), &len);

  assert_true(strcmp(value, expected) == 0,
              "expanding M_UNIQID macro failed; actual value:[%s], expected:[%s]",
              value,
              expected);
}

int main(int argc, char **argv)
{
  app_startup();

  UNIQID_TESTCASE(test_uniqid_template_format);

  app_shutdown();

  return 0;
}
