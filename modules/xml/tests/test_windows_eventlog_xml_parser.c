/*
 * Copyright (c) 2024 Attila Szakacs
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


#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#include "windows-eventlog-xml-parser.h"
#include "apphook.h"
#include "scratch-buffers.h"

void
setup(void)
{
  configuration = cfg_new_snippet();
  app_startup();
}

void
teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
  cfg_free(configuration);
}

TestSuite(windows_eventlog_xml_parser, .init = setup, .fini = teardown);

Test(windows_eventlog_xml_parser, test_windows_eventlog_xml_parser)
{
  const gchar *list_eventdata_data = \
                                     "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='EventCreate'/>"
                                     "<EventID Qualifiers='0'>999</EventID><Version>0</Version><Level>2</Level><Task>0</Task><Opcode>0</Opcode>"
                                     "<Keywords>0x80000000000000</Keywords><TimeCreated SystemTime='2024-01-12T09:30:12.1566754Z'/>"
                                     "<EventRecordID>934</EventRecordID><Correlation/><Execution ProcessID='0' ThreadID='0'/>"
                                     "<Channel>Application</Channel><Computer>DESKTOP-2MBFIV7</Computer>"
                                     "<Security UserID='S-1-5-21-3714454296-2738353472-899133108-1001'/></System>"
                                     "<RenderingInfo Culture='en-US'><Message>foobar</Message><Level>Error</Level><Task></Task><Opcode>Info</Opcode>"
                                     "<Channel></Channel><Provider></Provider><Keywords><Keyword>Classic</Keyword></Keywords></RenderingInfo>"
                                     "<EventData><Data>foo</Data><Data>bar</Data></EventData></Event>";

  const gchar *kv_list_eventdata_data = \
                                        "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='EventCreate'/>"
                                        "<EventID Qualifiers='0'>999</EventID><Version>0</Version><Level>2</Level><Task>0</Task><Opcode>0</Opcode>"
                                        "<Keywords>0x80000000000000</Keywords><TimeCreated SystemTime='2024-01-12T09:30:12.1566754Z'/>"
                                        "<EventRecordID>934</EventRecordID><Correlation/><Execution ProcessID='0' ThreadID='0'/>"
                                        "<Channel>Application</Channel><Computer>DESKTOP-2MBFIV7</Computer>"
                                        "<Security UserID='S-1-5-21-3714454296-2738353472-899133108-1001'/></System>"
                                        "<RenderingInfo Culture='en-US'><Message>foobar</Message><Level>Error</Level><Task></Task><Opcode>Info</Opcode>"
                                        "<Channel></Channel><Provider></Provider><Keywords><Keyword>Classic</Keyword></Keywords></RenderingInfo>"
                                        "<EventData><Data Name='param1'>foo</Data><Data Name='param2'>bar</Data></EventData></Event>";

  LogParser *parser = windows_eventlog_xml_parser_new(configuration);
  xml_parser_set_prefix(parser, ".winlog.");
  cr_assert(log_pipe_init(&parser->super));

  LogMessage *msg;
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;

  const gchar *value;
  gssize value_len;
  LogMessageValueType type;

  /* No attributes in Event.EventData.Data */
  msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_MESSAGE, list_eventdata_data, -1);
  cr_assert(log_parser_process_message(parser, &msg, &path_options));

  value = log_msg_get_value_by_name_with_type(msg, ".winlog.Event.EventData.Data", &value_len, &type);
  cr_assert_eq(type, LM_VT_LIST);
  cr_assert_str_eq(value, "foo,bar");

  value = log_msg_get_value_by_name_with_type(msg, ".winlog.Event.System.EventID", &value_len, &type);
  cr_assert_eq(type, LM_VT_STRING);
  cr_assert_str_eq(value, "999");

  log_msg_unref(msg);

  /* Attributes in Event.EventData.Data */
  msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_MESSAGE, kv_list_eventdata_data, -1);
  cr_assert(log_parser_process_message(parser, &msg, &path_options));

  value = log_msg_get_value_by_name_with_type(msg, ".winlog.Event.EventData.Data.param1", &value_len, &type);
  cr_assert_eq(type, LM_VT_STRING);
  cr_assert_str_eq(value, "foo");

  value = log_msg_get_value_by_name_with_type(msg, ".winlog.Event.EventData.Data.param2", &value_len, &type);
  cr_assert_eq(type, LM_VT_STRING);
  cr_assert_str_eq(value, "bar");

  value = log_msg_get_value_by_name_with_type(msg, ".winlog.Event.EventData.Data", &value_len, &type);
  cr_assert_eq(type, LM_VT_NULL);
  cr_assert_str_eq(value, "");

  value = log_msg_get_value_by_name_with_type(msg, ".winlog.Event.EventData.Data._Name", &value_len, &type);
  cr_assert_eq(type, LM_VT_NULL);
  cr_assert_str_eq(value, "");

  value = log_msg_get_value_by_name_with_type(msg, ".winlog.Event.System.EventID", &value_len, &type);
  cr_assert_eq(type, LM_VT_STRING);
  cr_assert_str_eq(value, "999");

  log_msg_unref(msg);

  cr_assert(log_pipe_deinit(&parser->super));
  cr_assert(log_pipe_unref(&parser->super));
}
