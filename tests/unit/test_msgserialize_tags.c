/*
 * Copyright (c) 2002-2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2012 Balázs Scheidler
 * Copyright (c) 2012-2013 Viktor Juhasz
 * Copyright (c) 2012-2013 Viktor Tusa
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

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


char tag_larger_than_256[257];

void
set_tag_larger_than_256()
{
  int i;

  for (i = 0; i < 257; i++)
    {
      tag_larger_than_256[i] = 'a';
    }
  tag_larger_than_256[256] = '\0';
};

int
main(int argc, char **argv)
{
  SerializeArchive *sa;
  LogMessage *msg, *read_msg;
  GString *serialized;
  char t[3] = "aa";

  const gchar *msg_template = "<5> localhost test: test message template";

  set_tag_larger_than_256();
  app_startup();

  configuration = cfg_new(0x0302);
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
  log_msg_set_tag_by_name(msg, tag_larger_than_256);

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
  serialized = g_string_sized_new(2048);
  sa = serialize_string_archive_new(serialized);

  log_msg_serialize(msg, sa);

  /* Reinit tags */
  log_tags_global_deinit();
  log_tags_global_init();

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
  log_msg_deserialize(read_msg, sa);

  TEST_ASSERT(! strcmp("d", log_tags_get_by_id(log_tags_get_by_name("d"))));

  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "bca") == FALSE);
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "a") == TRUE);
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "b") == TRUE);
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "c") == TRUE);
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "d") == TRUE);
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "e") == FALSE);
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, "f") == FALSE);
  TEST_ASSERT(log_msg_is_tag_by_name(read_msg, tag_larger_than_256) == TRUE);


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
