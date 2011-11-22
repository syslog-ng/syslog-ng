#include "syslog-ng.h"
#include "logmsg.h"
#include "templates.h"
#include "misc.h"
#include "apphook.h"
#include "cfg.h"
#include "timeutils.h"
#include "plugin.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

gboolean verbose = FALSE;
MsgFormatOptions parse_options;
LogTemplate *rcptid_template;

#define TEST_ASSERT(x, format, value, expected)   \
  do        \
    {         \
        if (!(x))   \
          {     \
            fprintf(stderr, "Testcase failed; msg='%s', cond='%s', value=" format ", expected=" format "\n", message, #x, value, expected); \
            exit(1);    \
          }     \
    }       \
  while (0)

void
testcase(gchar *message)
{
  extern RcptidState g_rcptidstate;
  GString *serialized;
  LogMessage *cloned;
  LogMessage *msg;
  SerializeArchive *sa;
  LogTemplate *template;
  PersistState *state;

  unlink("test_values.persist");
  state = persist_state_new("test_values.persist");
  if (!persist_state_start(state))
    {
      fprintf(stderr, "Error starting persist_state object\n");
      exit(1);
    }
  log_msg_init_rctpid(state);
  msg = log_msg_new(message,strlen(message),NULL,&parse_options);
  log_msg_create_rcptid(msg);

  /* Test that the counter starts from 1 */
  TEST_ASSERT(msg->rcptid == 1,"%"G_GUINT64_FORMAT,msg->rcptid,1);

  serialized = g_string_sized_new(0);
  sa = serialize_string_archive_new(serialized);
  log_msg_write(msg, sa);
  serialize_archive_free(sa);

  /* read it back */
  cloned = log_msg_new_empty();
  sa = serialize_string_archive_new(serialized);
  log_msg_read(cloned, sa);
  serialize_archive_free(sa);

  /* Test the serialization handle the rcptid */
  TEST_ASSERT(cloned->rcptid == 1,"%"G_GUINT64_FORMAT,cloned->rcptid,1);

  g_rcptidstate.g_rcptid=0x0000FFFFFFFFFFFE;
  log_msg_create_rcptid(msg);
  /* Test the persist handling */
  TEST_ASSERT(msg->rcptid == 0x0000FFFFFFFFFFFE,"%"G_GUINT64_FORMAT,msg->rcptid,0x0000FFFFFFFFFFFE);

  persist_state_commit(state);
  persist_state_free(state);
  g_rcptidstate.g_rcptid=0;
  state = persist_state_new("test_values.persist");
  if (!persist_state_start(state))
    {
      fprintf(stderr, "Error starting persist_state object\n");
      exit(1);
    }
  log_msg_init_rctpid(state);

  log_msg_create_rcptid(msg);
  TEST_ASSERT(msg->rcptid == 0x0000FFFFFFFFFFFF,"%"G_GUINT64_FORMAT,msg->rcptid,0x0000FFFFFFFFFFFF);
  /* End of testing persist handling */

  log_msg_create_rcptid(msg);

  /* Test the counter overflow */
  TEST_ASSERT(msg->rcptid == 1,"%"G_GUINT64_FORMAT,msg->rcptid,1);

  log_template_format(rcptid_template,msg,NULL,0,0,NULL,serialized);
  /* Test the rcptid macro with valid value */
  TEST_ASSERT(strcmp(serialized->str,"RCPTID: 1 MESSAGE: This is a test message") == 0,"%s",serialized->str,"RPCTID: 1 MESSAGE: This is a test message");
  log_msg_unref(msg);

  msg = log_msg_new(message,strlen(message),NULL,&parse_options);
  log_template_format(rcptid_template,msg,NULL,0,0,NULL,serialized);
  /* Test the rcptid macro with invalid value */
  TEST_ASSERT(strcmp(serialized->str,"RCPTID:  MESSAGE: This is a test message") == 0,"%s",serialized->str,"RPCTID:  MESSAGE: This is a test message");

  g_string_free(serialized, TRUE);
  return;
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  LogMessage *msg;
  PersistState *state;
  GError *error = NULL;
#if __hpux__
  return 0;
#endif
  char *msg_str = "<155>2006-02-11T10:34:56+01:00 bzorp syslog-ng[23323]:This is a test message";

  if (argc > 1)
    verbose = TRUE;

  configuration = cfg_new(0x0302);
  plugin_load_module("syslogformat", configuration, NULL);
  plugin_load_module("basicfuncs", configuration, NULL);
  plugin_load_module("convertfuncs", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
  configuration->template_options.frac_digits = 3;
  configuration->template_options.time_zone_info[LTZ_LOCAL] = time_zone_info_new(NULL);
  app_startup();
  rcptid_template = log_template_new(configuration,NULL);
  if (!log_template_compile(rcptid_template,"RCPTID: $RCPTID MESSAGE: $MSG",&error))
    {
      return 1;
    }
  testcase(msg_str);
  return 0;
}
