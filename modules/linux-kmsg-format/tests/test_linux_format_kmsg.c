/*
 * Copyright (c) 2012-2013 Balabit
 * Copyright (c) 2012-2013 Gergely Nagy <algernon@balabit.hu>
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

#include "syslog-ng.h"

#include "testutils.h"
#include "msg_parse_lib.h"
#include "apphook.h"
#include "plugin.h"
#include "plugin-types.h"

static LogMessage *
kmsg_parse_message(const gchar *raw_message_str)
{
  LogMessage *message;
  GSockAddr *addr = g_sockaddr_inet_new("10.10.10.10", 1010);

  message = log_msg_new(raw_message_str, strlen(raw_message_str), addr, &parse_options);

  g_sockaddr_unref(addr);
  return message;
}

static void
assert_log_kmsg_value(LogMessage *message, const gchar *key,
                      const gchar *expected_value)
{
  const gchar *actual_value = log_msg_get_value(message,
                                                log_msg_get_value_handle(key),
                                                NULL);
  assert_string(actual_value, expected_value, NULL);
}

void
test_kmsg_single_line(void)
{
  gchar msg[] =
    "5,2,1;Linux version 3.5-trunk-amd64 (Debian 3.5.2-1~experimental.1) (debian-kernel@lists.debian.org) (gcc version 4.6.3 (Debian 4.6.3-1) ) #1 SMP Mon Aug 20 04:17:46 UTC 2012\n";
  LogMessage *parsed_message;

  testcase_begin("Testing single-line /dev/kmsg parsing; msg='%s'", msg);

  parsed_message = kmsg_parse_message(msg);

  assert_guint16(parsed_message->pri, 5, "Unexpected message priority");
  assert_log_message_value(parsed_message, LM_V_MSGID, "2");
  msg[sizeof(msg) - 2] = '\0';
  assert_log_message_value(parsed_message, LM_V_MESSAGE, msg + 6);
  assert_log_kmsg_value(parsed_message, ".linux.timestamp", "1");

  log_msg_unref(parsed_message);

  testcase_end();
}

void
test_kmsg_multi_line(void)
{
  gchar msg[] = "6,202,98513;pci_root PNP0A08:00: host bridge window [io  0x0000-0x0cf7]\n" \
                " SUBSYSTEM=acpi\n" \
                " DEVICE=+acpi:PNP0A08:00\n";
  LogMessage *parsed_message;

  testcase_begin("Testing multi-line /dev/kmsg parsing; msg='%s'", msg);

  parsed_message = kmsg_parse_message(msg);

  assert_guint16(parsed_message->pri, 6, "Unexpected message priority");
  assert_log_message_value(parsed_message, LM_V_MSGID, "202");
  assert_log_message_value(parsed_message, LM_V_MESSAGE, "pci_root PNP0A08:00: host bridge window [io  0x0000-0x0cf7]");
  assert_log_kmsg_value(parsed_message, ".linux.SUBSYSTEM", "acpi");
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.type", "acpi");
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.name", "PNP0A08:00");

  log_msg_unref(parsed_message);

  testcase_end();
}

void
test_kmsg_with_extra_fields(void)
{
  gchar msg[] = "5,2,0,some extra field,3,4,5;And this is the real message\n";
  LogMessage *parsed_message;

  testcase_begin("Testing /dev/kmsg parsing, with extra fields; msg='%s'", msg);

  parsed_message = kmsg_parse_message(msg);
  assert_guint16(parsed_message->pri, 5, "Unexpected message priority");
  assert_log_message_value(parsed_message, LM_V_MSGID, "2");
  assert_log_message_value(parsed_message, LM_V_MESSAGE, "And this is the real message");

  log_msg_unref(parsed_message);

  testcase_end();
}

void
test_kmsg_device_parsing(void)
{
  gchar msg_subsys[] = "6,202,98513;pci_root PNP0A08:00: host bridge window [io  0x0000-0x0cf7]\n" \
                       " SUBSYSTEM=acpi\n" \
                       " DEVICE=+acpi:PNP0A08:00\n";
  gchar msg_block[] = "6,202,98513;Fake message\n" \
                      " DEVICE=b12:1\n";
  gchar msg_char[] = "6,202,98513;Fake message\n" \
                     " DEVICE=c3:4\n";
  gchar msg_netdev[] = "6,202,98513;Fake message\n" \
                       " DEVICE=n8\n";
  gchar msg_unknown[] = "6,202,98513;Fake message\n" \
                        " DEVICE=w12345\n";
  gchar msg_invalid_block[] = "6,202;Fake message\n" \
                              " DEVICE=b12:1\n";
  LogMessage *parsed_message;

  testcase_begin("Testing /dev/kmsg DEVICE= parsing");

  parsed_message = kmsg_parse_message(msg_subsys);
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.type", "acpi");
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.name", "PNP0A08:00");
  log_msg_unref(parsed_message);

  parsed_message = kmsg_parse_message(msg_block);
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.type", "block");
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.major", "12");
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.minor", "1");
  log_msg_unref(parsed_message);

  parsed_message = kmsg_parse_message(msg_char);
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.type", "char");
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.major", "3");
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.minor", "4");
  log_msg_unref(parsed_message);

  parsed_message = kmsg_parse_message(msg_netdev);
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.type", "netdev");
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.index", "8");
  log_msg_unref(parsed_message);

  parsed_message = kmsg_parse_message(msg_unknown);
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.type", "<unknown>");
  assert_log_kmsg_value(parsed_message, ".linux.DEVICE.name", "w12345");
  log_msg_unref(parsed_message);

  parsed_message = kmsg_parse_message(msg_invalid_block);
  assert_log_message_value(parsed_message, LM_V_MESSAGE,
                           "Error processing log message: 6,202>@<;Fake message\n DEVICE=b12:1");
  log_msg_unref(parsed_message);

  testcase_end();
}

void
init_and_load_kmsgformat_module(void)
{
  configuration = cfg_new_snippet();
  cfg_load_module(configuration, "linux-kmsg-format");
  parse_options.format = "linux-kmsg";

  msg_format_options_defaults(&parse_options);
  msg_format_options_init(&parse_options, configuration);
}

void
deinit_kmsgformat_module(void)
{
  if (configuration)
    cfg_free(configuration);
  configuration = NULL;
  parse_options.format = NULL;
  parse_options.format_handler = NULL;
}

int
main(void)
{
  app_startup();
  setenv("TZ", "UTC", TRUE);
  tzset();

  init_and_load_kmsgformat_module();

  test_kmsg_single_line();
  test_kmsg_multi_line();
  test_kmsg_with_extra_fields();
  test_kmsg_device_parsing();

  deinit_kmsgformat_module();
  app_shutdown();
}
