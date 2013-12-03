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
#include "apphook.h"
#include "logmsg.h"
#include "logmsg-serialize.h"
#include "cfg.h"
#include "plugin.h"
#include "msg_parse_lib.h"
#include "testutils.h"

#define RAW_MSG "<132>1 2006-10-29T01:59:59.156+01:00 mymachine evntslog - - [exampleSDID@0 iut=\"3\" eventSource=\"Application\"] An application event log entry..."
#define ERROR_MSG "Failed at %s(%d)", __FILE__, __LINE__

void
test_msg_serialize()
{
  LogMessage *msg = NULL;
  LogTemplate *template = NULL;
  GString *stream = g_string_new("");
  GString *output = g_string_new("");
  NVHandle test_handle = 0;
  SerializeArchive *sa = serialize_string_archive_new(stream);

  parse_options.flags |= LP_SYSLOG_PROTOCOL;
  log_msg_registry_init();
  test_handle = log_msg_get_value_handle("aaa");

  template = log_template_new(configuration, NULL);
  msg = log_msg_new(RAW_MSG, strlen(RAW_MSG), NULL, &parse_options);
  log_msg_set_value(msg, test_handle, "test_value", -1);
  log_template_compile(template, "${.SDATA.exampleSDID@0.eventSource}", NULL);
  log_template_append_format(template, msg, &configuration->template_options, LTZ_SEND, 0, NULL, output);
  assert_string(output->str, "Application", ERROR_MSG);

  log_msg_serialize(msg, sa);
  log_msg_unref(msg);
  log_template_unref(template);
  g_string_free(output, TRUE);
  log_msg_registry_deinit();

  log_msg_registry_init();

  output = g_string_new("");
  template = log_template_new(configuration, NULL);
  log_template_compile(template, "${.SDATA.exampleSDID@0.eventSource}", NULL);
  msg = log_msg_new_empty();

  assert_true(log_msg_deserialize(msg, sa), ERROR_MSG);
  log_template_append_format(template, msg, &configuration->template_options, LTZ_SEND, 0, NULL, output);
  assert_string(output->str, "Application", ERROR_MSG);
  log_msg_unref(msg);
  serialize_archive_free(sa);
  log_template_unref(template);
  g_string_free(stream, TRUE);
  g_string_free(output, TRUE);
  log_msg_registry_deinit();
}

int
main(int argc, char **argv)
{
  app_startup();
  init_and_load_syslogformat_module();
  log_template_global_init();
  test_msg_serialize();
  deinit_syslogformat_module();
  return 0;
}
