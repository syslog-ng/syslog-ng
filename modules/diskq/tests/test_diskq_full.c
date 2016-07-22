/*
 * Copyright (c) 2002-2016 Balabit
 * Copyright (c) 2016 Viktor Juhasz <viktor.juhasz@balabit.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "messages.h"
#include "logqueue-disk.h"
#include "logqueue-disk-reliable.h"
#include "logqueue-disk-non-reliable.h"
#include "apphook.h"
#include "plugin.h"
#include "testutils.h"
#include "queue_utils_lib.h"
#include "test_diskq_tools.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DISKQ_FILENAME "test_become_full.qf"

MsgFormatOptions parse_options;


static void msg_post_function(LogMessage *msg)
{
  log_msg_unref(msg);
}

void
test_diskq_become_full(gboolean reliable)
{
  LogQueue *q;
  acked_messages = 0;
  fed_messages = 0;
  DiskQueueOptions options = {0};

  const gchar *persist_name = "test_diskq";

  options.reliable = reliable;
  if (reliable)
    {
      _construct_options(&options, 1000, 1000, reliable);
      q = log_queue_disk_reliable_new(&options, persist_name);
    }
  else
    {
      _construct_options(&options, 1000, 0, reliable);
      q = log_queue_disk_non_reliable_new(&options, persist_name);
    }

  log_queue_set_use_backlog(q, TRUE);

  stats_lock();
  StatsClusterKey sc_key;
  stats_cluster_logpipe_key_set(&sc_key, SCS_DESTINATION, q->persist_name, NULL );
  stats_register_counter(0, &sc_key, SC_TYPE_DROPPED, &q->dropped_messages);
  stats_counter_set(q->dropped_messages, 0);
  stats_unlock();
  unlink(DISKQ_FILENAME);
  log_queue_disk_load_queue(q, DISKQ_FILENAME);
  feed_some_messages(q, 1000, &parse_options);

  assert_gint(q->dropped_messages->value, 1000, "Bad dropped message number (reliable: %s)", reliable ? "TRUE" : "FALSE");

  log_queue_unref(q);
  disk_queue_options_destroy(&options);
  unlink(DISKQ_FILENAME);
}

int
main(void)
{
  app_startup();
  setenv("TZ", "MET-1METDST", TRUE);
  tzset();

  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "syslogformat");
  cfg_load_module(configuration, "disk-buffer");
  cfg_load_module(configuration, "builtin-serializer");
  msg_set_post_func(msg_post_function);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
  test_diskq_become_full(TRUE);
  test_diskq_become_full(FALSE);
  return 0;
}
