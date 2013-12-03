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
#include "cfg.h"
#include "plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define BENCHMARK_COUNT 100000

MsgFormatOptions parse_options;

int
benchmark(void)
{
  SerializeArchive *sa;
  LogMessage *msg;
  gint i;
  GTimeVal start, end;
  LogTemplate *template;
  const gchar *msg_template = "<5> localhost test: test message template";

  configuration = cfg_new(0x0302);
  plugin_load_module("syslogformat", configuration, NULL);
  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
  parse_options.flags = 0;
  parse_options.bad_hostname = NULL;
  msg = log_msg_new(msg_template, strlen(msg_template), NULL, &parse_options);

  g_get_current_time(&start);
  for (i = 0; i < BENCHMARK_COUNT; i++)
    {
      GString *serialized;

      serialized = g_string_sized_new(1024);
      sa = serialize_string_archive_new(serialized);

      log_msg_serialize(msg, sa);
      serialize_archive_free(sa);
      g_string_free(serialized, TRUE);
    }
  g_get_current_time(&end);
  printf("time required to serialize %d elements: %ld\n", i, g_time_val_diff(&end, &start));

  template = log_template_new(configuration, NULL);
  log_template_compile(template, "$DATE $HOST $MSGHDR$MSG\n", NULL);
  g_get_current_time(&start);
  for (i = 0; i < BENCHMARK_COUNT; i++)
    {
      GString *formatted = g_string_sized_new(1024);

      log_template_format(template, msg, NULL, LTZ_LOCAL, 0, NULL, formatted);
      g_string_free(formatted, TRUE);
    }
  g_get_current_time(&end);
  printf("time required to format %d elements: %ld\n", i, g_time_val_diff(&end, &start));

  log_msg_unref(msg);

  return 0;
}

int
main(void)
{
  app_startup();
  benchmark();
  app_shutdown();
  return 0;
}
