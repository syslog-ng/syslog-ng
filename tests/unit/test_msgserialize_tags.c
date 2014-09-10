#include "syslog-ng.h"
#include "serialize.h"
#include "misc.h"
#include "logmsg.h"
#include "logmsg-serialize.h"
#include "apphook.h"
#include "timeutils.h"
#include "tags.h"
#include "cfg.h"
#include "plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

MsgFormatOptions parse_options;

#define TEST_ASSERT(x)  \
  do { \
   if (!(x)) \
     { \
       fprintf(stderr, "test assertion failed: " #x " line: %d\n", __LINE__); \
       return 1; \
     } \
  } while (0)

int
main()
{
  SerializeArchive *sa;
  LogMessage *msg, *read_msg;
  GString *serialized;
  char t[3] = "aa";

  const gchar *msg_template = "<5> localhost test: test message template";

  app_startup();

  configuration = cfg_new(VERSION_VALUE_3_2);
  plugin_load_module("syslogformat", configuration, NULL);

  /* Set up the message */
  parse_options.flags = 0;
  parse_options.bad_hostname = NULL;
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
  msg = log_msg_new(msg_template, strlen(msg_template), NULL, &parse_options);
  TEST_ASSERT(msg->num_tags == 0);


  log_msg_set_tag_by_name(msg, "a");
  log_msg_set_tag_by_name(msg, "b");
  log_msg_set_tag_by_name(msg, "c");

  /* Loop is required because in this case the tags are represented on
     multiple integers within message 'msg' */
  for (;t[0] < 'c'; ++t[0])
    for (t[1] = 'a'; t[1] <='z'; ++t[1])
      log_tags_get_by_name(t);

  log_msg_set_tag_by_name(msg, "d");

  TEST_ASSERT(log_msg_is_tag_by_name(msg, "a") == TRUE);
  TEST_ASSERT(log_msg_is_tag_by_name(msg, "d") == TRUE);
  TEST_ASSERT(log_msg_is_tag_by_name(msg, "bb") == FALSE);

  /* Serialize */
  serialized = g_string_sized_new(1024);
  sa = serialize_string_archive_new(serialized);

  log_msg_write(msg, sa);

  /* Reinit tags */
  log_tags_deinit();
  log_tags_init();

  read_msg = log_msg_new_empty();

  TEST_ASSERT(! strcmp("c", log_tags_get_by_id(log_tags_get_by_name("c"))));
  TEST_ASSERT(! strcmp("bca", log_tags_get_by_id(log_tags_get_by_name("bca"))));
  TEST_ASSERT(! strcmp("b", log_tags_get_by_id(log_tags_get_by_name("b"))));
  TEST_ASSERT(! strcmp("a", log_tags_get_by_id(log_tags_get_by_name("a"))));

  for (t[0] = 'a'; t[0] < 'd'; ++t[0])
    for (t[1] = 'a'; t[1] <='z'; ++t[1])
      log_tags_get_by_name(t);

  TEST_ASSERT(! strcmp("e", log_tags_get_by_id(log_tags_get_by_name("e"))));
  /* Unserialize:
     the new tag, called as "d", is created here
   */
  log_msg_read(read_msg, sa);

  TEST_ASSERT(! strcmp("d", log_tags_get_by_id(log_tags_get_by_name("d"))));

  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "bca") == FALSE);
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "a") == TRUE);
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "b") == TRUE);
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "c") == TRUE);
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "d") == TRUE);
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "e") == FALSE);
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "f") == FALSE);

  /* A new tag, newly defined */
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "g") == FALSE);

  for (t[0] = 'a'; t[0] < 'd'; ++t[0])
    for (t[1] = 'a'; t[1] <='z'; ++t[1])
      TEST_ASSERT(log_msg_is_tag_by_name(read_msg, t) == FALSE);

  /* Shutdown */
  log_msg_unref(msg);
  log_msg_unref(read_msg);

  app_shutdown();
  return 0;
}
