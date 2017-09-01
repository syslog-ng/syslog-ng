/*
 * Copyright (c) 2017 Balabit
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
#include "msg_parse_lib.h"
#include "xml.h"
#include "apphook.h"
#include "scratch-buffers.h"

void
setup(void)
{
  configuration = cfg_new_snippet(0x0390);
  app_startup();
}

void
teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
  cfg_free(configuration);
}

TestSuite(xmlparser, .init = setup, .fini = teardown);

typedef struct
{
  const gchar *input;
  const gchar *key;
  const gchar *value;
} XMLTestCase;

ParameterizedTestParameters(xmlparser, valid_inputs)
{
  static XMLTestCase test_cases[] =
  {
    {"<tag1>value1</tag1>", ".xml.tag1", "value1"},
    {"<tag1 attr='attr_value'>value1</tag1>", ".xml.tag1._attr", "attr_value"},
    {"<tag1><tag2>value2</tag2></tag1>", ".xml.tag1.tag2", "value2"},
    {"<tag1>part1<tag2>value2</tag2>part2</tag1>", ".xml.tag1", "part1part2"},
    {"<tag1><tag11></tag11><tag12><tag121>value</tag121></tag12></tag1>", ".xml.tag1.tag12.tag121", "value"},
    {"<tag1><tag11></tag11><tag12><tag121 attr1='1' attr2='2'>value</tag121></tag12></tag1>", ".xml.tag1.tag12.tag121._attr1", "1"},
    {"<tag1><tag11></tag11><tag12><tag121 attr1='1' attr2='2'>value</tag121></tag12></tag1>", ".xml.tag1.tag12.tag121._attr2", "2"},
  };

  return cr_make_param_array(XMLTestCase, test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
}


ParameterizedTest(XMLTestCase *test_cases, xmlparser, valid_inputs)
{
  LogParser *xml_parser = xml_parser_new(configuration);
  log_pipe_init((LogPipe *)xml_parser);

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_MESSAGE, test_cases->input, -1);

  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  log_parser_process_message(xml_parser, &msg, &path_options);

  const gchar *value = log_msg_get_value_by_name(msg, test_cases->key, NULL);

  cr_assert_str_eq(value, test_cases->value, "key: %s | value: %s != %s (expected)", test_cases->key, value, test_cases->value);

  log_pipe_deinit((LogPipe *)xml_parser);
  log_pipe_unref((LogPipe *)xml_parser);
  log_msg_unref(msg);
}
