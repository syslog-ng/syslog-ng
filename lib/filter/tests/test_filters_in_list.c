#include <stdlib.h>
#include <glib.h>

#include "cfg.h"
#include "messages.h"
#include "syslog-names.h"
#include "logmsg.h"
#include "apphook.h"
#include "plugin.h"
#include "filter/filter-in-list.h"

#include "testutils.h"

static MsgFormatOptions parse_options;

static LogMessage *
create_test_message (void)
{
  const gchar *log_message = "<15>Sep  4 15:03:55 localhost test-program[3086]: some random message";

  return log_msg_new(log_message, strlen(log_message), NULL, &parse_options);
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  LogMessage *msg;
  FilterExprNode *filter_node;
  char *top_srcdir = getenv("top_srcdir");
  gchar *list_file;

  app_startup();

  configuration = cfg_new(0x0305);
  plugin_load_module("syslogformat", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);

  assert_not_null(top_srcdir, "The $top_srcdir environment variable MUST NOT be empty!");

  list_file = g_strdup_printf("%s/lib/filter/tests/test.list", top_srcdir);
  filter_node = filter_in_list_new(list_file, "PROGRAM");
  assert_not_null(filter_node, "Constructing an in-list filter");
  g_free (list_file);

  msg = create_test_message();

  assert_true(filter_expr_eval(filter_node, msg), "in-list filter matches");

  app_shutdown();

  return 0;
}
