/*
 * Copyright (c) 2015 Gergely Nagy
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
 */
#include "syslog-ng.h"

#include "testutils.h"
#include "msg_parse_lib.h"
#include "apphook.h"
#include "plugin.h"
#include "plugin-types.h"

static LogMessage *
snmp_parse_message(const gchar *raw_message_str)
{
  LogMessage *message;
  GSockAddr *addr = g_sockaddr_inet_new("10.10.10.10", 1010);

  message = log_msg_new(raw_message_str, strlen(raw_message_str), addr, &parse_options);

  g_sockaddr_unref(addr);
  return message;
}

static void
assert_log_snmp_value(LogMessage *message, const gchar *key,
                      const gchar *expected_value)
{
  const gchar *actual_value = log_msg_get_value(message,
                                                log_msg_get_value_handle(key),
                                                NULL);
  assert_string(actual_value, expected_value, NULL);
}

void
test_snmp_format_parses_well_formed_traps_and_puts_result_in_message(void)
{
  LogMessage *parsed_message;
  gchar msg[] = "foo = STRING: \"blah\"";

  testcase_begin("Testing well formed SNMP trap parsing; msg='%s'", msg);

  parsed_message = snmp_parse_message(msg);

  assert_log_snmp_value(parsed_message, ".snmp.foo", "blah");

  log_msg_unref(parsed_message);

  testcase_end();
}

static void
test_snmp_format_parses_well_formed_traps_regardless_of_whitespace(void)
{
  LogMessage *parsed_message;
  gchar msg[] = "foo= STRING: \"blah\"    bar =STRING: \"baz\" baz=STRING:\"hello\"";

  testcase_begin("Testing well formed SNMP trap parsing, regardless of whitespace; msg='%s'", msg);

  parsed_message = snmp_parse_message(msg);

  assert_log_snmp_value(parsed_message, ".snmp.foo", "blah");
  assert_log_snmp_value(parsed_message, ".snmp.bar", "baz");
  assert_log_snmp_value(parsed_message, ".snmp.baz", "hello");

  log_msg_unref(parsed_message);
  testcase_end();
}

static void
test_snmp_format_fails_for_invalid_trap_message(void)
{
  LogMessage *msg;

  testcase_begin("Testing that invalid SNMP traps produce a parse error");

  msg = snmp_parse_message("not-valid-trap-message");
  assert_log_snmp_value(msg, "MESSAGE", "Error processing log message: not-valid-trap-message");
  log_msg_unref(msg);

  msg = snmp_parse_message("not-valid-trap-message = value");
  assert_log_snmp_value(msg, "MESSAGE", "Error processing log message: not-valid-trap-message = value");
  log_msg_unref(msg);

  msg = snmp_parse_message("not-valid-trap-message = type value");
  assert_log_snmp_value(msg, "MESSAGE", "Error processing log message: not-valid-trap-message = type value");
  log_msg_unref(msg);

  testcase_end();
}

static void
test_snmp_format_validate_type_representation(void)
{
  LogMessage *parsed_message;
  gchar msg[] = "string = STRING: \"blah\" int = INT32: 42";

  testcase_begin("Testing SNMP trap type parsing; msg='%s'", msg);

  parsed_message = snmp_parse_message(msg);

  assert_log_snmp_value(parsed_message, ".snmp.int", "42");
  assert_log_snmp_value(parsed_message, ".snmp.string", "blah");

  log_msg_unref(parsed_message);
  testcase_end();
}

static void
init_and_load_snmp_module()
{
  configuration = cfg_new(VERSION_VALUE);
  plugin_load_module("snmp-format", configuration, NULL);
  parse_options.format = "snmp";

  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
}

static void
deinit_snmp_module()
{
  if (configuration)
    cfg_free(configuration);

  configuration = NULL;
  parse_options.format = NULL;
  parse_options.format_handler = NULL;
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();
  putenv("TZ=UTC");
  tzset();

  init_and_load_snmp_module();

  test_snmp_format_parses_well_formed_traps_and_puts_result_in_message();
  test_snmp_format_parses_well_formed_traps_regardless_of_whitespace();
  test_snmp_format_validate_type_representation();
  test_snmp_format_fails_for_invalid_trap_message();

  deinit_snmp_module();
  app_shutdown();
  return 0;
}
