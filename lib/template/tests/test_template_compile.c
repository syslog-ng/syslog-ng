/*
 * Copyright (c) 2013-2014 Balabit
 * Copyright (c) 2013-2014 Balázs Scheidler <balazs.scheidler@balabit.com>
 * Copyright (c) 2013 Viktor Juhasz <viktor.juhasz@balabit.com>
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

#include "template/templates.c"
#include "template/simple-function.h"
#include "logmsg/logmsg.h"
#include "testutils.h"
#include "cfg.h"
#include "plugin.h"

static void
hello(LogMessage *msg, int argc, GString *argv[], GString *result)
{
  return;
}

TEMPLATE_FUNCTION_SIMPLE(hello);
Plugin hello_plugin = TEMPLATE_FUNCTION_PLUGIN(hello, "hello");


#define assert_common_element(expected) \
    assert_string(current_elem->text, expected.text, ASSERTION_ERROR("Bad compiled template text")); \
    if (expected.default_value) \
      { \
        assert_string(current_elem->default_value, expected.default_value, ASSERTION_ERROR("Bad compiled template default value")); \
      } \
    else \
      { \
        assert_gpointer(current_elem->default_value, NULL, ASSERTION_ERROR("Bad compiled template default value")); \
      } \
    assert_gint(current_elem->msg_ref, expected.msg_ref, ASSERTION_ERROR("Bad compiled template msg_ref"));

#define fill_expected_template_element(element, text, default_value, spec, type, msg_ref)  {\
    element.text; \
    element.default_value; \
    element.spec; \
    element.type;  \
    element.msg_ref; }

#define assert_compiled_template(text, default_value, spec, type, msg_ref) \
  do { \
    LogTemplateElem expected_elem;                                \
                                                                                                                                                                \
    fill_expected_template_element(expected_elem, text, default_value, spec, type, msg_ref);                  \
    assert_gint((current_elem->type), (expected_elem.type), ASSERTION_ERROR("Bad compiled template type"));               \
    assert_common_element(expected_elem);                               \
    if ((expected_elem.type) == LTE_MACRO) assert_gint(current_elem->macro, expected_elem.macro, ASSERTION_ERROR("Bad compiled template macro"));     \
    if ((expected_elem.type) == LTE_VALUE) assert_gint(current_elem->value_handle, expected_elem.value_handle, ASSERTION_ERROR("Bad compiled template macro")); \
    if ((expected_elem.type) == LTE_FUNC) assert_gpointer(current_elem->func.ops, expected_elem.func.ops, ASSERTION_ERROR("Bad compiled template macro"));  \
  } while (0)


#define TEMPLATE_TESTCASE(x, ...) do { template_testcase_begin(#x, #__VA_ARGS__); x(__VA_ARGS__); template_testcase_end(); } while(0)

#define template_testcase_begin(func, args)                    \
  do                                                            \
    {                                                           \
      testcase_begin("%s(%s)", func, args);                     \
      template = log_template_new(configuration, NULL);   \
      start_grabbing_messages();        \
    }                                                           \
  while (0)

#define template_testcase_end()                                \
  do                                                            \
    {                                                           \
      log_template_unref(template);       \
      template = NULL;            \
      testcase_end();                                           \
    }                                                           \
  while (0)


static LogTemplate *template;
static GList *current_elem_list;
static LogTemplateElem *current_elem;

static void
select_current_element(void)
{
  current_elem = (LogTemplateElem *) current_elem_list->data;
}

static void
select_first_element(void)
{
  current_elem_list = template->compiled_template;
  select_current_element();
}

static void
select_next_element(void)
{
  current_elem_list = current_elem_list->next;
  select_current_element();
}

static inline void
assert_template_compile(const gchar *template_string)
{
  GError *error = NULL;

  assert_true(log_template_compile(template, template_string, &error), ASSERTION_ERROR("Can't compile template"));
  assert_string(template->template, template_string, ASSERTION_ERROR("Bad stored template"));
  select_first_element();
}

static inline void
assert_failed_template_compile(const gchar *template_string, const gchar *expected_error_message)
{
  GError *error = NULL;

  assert_false(log_template_compile(template, template_string, &error), ASSERTION_ERROR("Can compile bad template"));
  assert_string(error->message, expected_error_message, ASSERTION_ERROR("Bad error message"));

  g_clear_error(&error);
  select_first_element();
}

static gpointer
get_template_function_ops(const gchar *name)
{
  Plugin *plugin;

  plugin = cfg_find_plugin(configuration, LL_CONTEXT_TEMPLATE_FUNC, name);
  assert_not_null(plugin, "Template function %s is not found", name);

  if (plugin)
    return plugin->construct(plugin);
  return NULL;
}

static void
test_simple_string_literal(void)
{
  const gchar *text_ = "Test String";

  assert_template_compile(text_);
  assert_compiled_template(text = text_, default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_simple_macro(void)
{
  assert_template_compile("${MESSAGE}");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_macro_and_text(void)
{

  assert_template_compile("${MESSAGE}test value");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);

  select_next_element();
  assert_compiled_template(text = "test value", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_macro_without_braces(void)
{
  assert_template_compile("$MESSAGE");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_macro_name_without_braces_are_terminated_with_non_identifier_characters(void)
{
  /* macro names consist of [A-Z0-9_] */
  assert_template_compile("$MESSAGE test value");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);

  select_next_element();
  assert_compiled_template(text = " test value", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_macro_without_at_records_that_no_msgref_was_present_by_msgref_zero(void)
{
  assert_template_compile("${MESSAGE}");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_macro_with_at_references_a_single_msg_in_the_context_stack_by_setting_msgref(void)
{
  assert_template_compile("${MESSAGE}@0");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 1);

  assert_template_compile("${MESSAGE}@1");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 2);

}

static void
test_macro_with_invalid_msgref_are_recognized_as_the_top_element_in_the_stack(void)
{
  assert_template_compile("${MESSAGE}@gmail.com");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);

  select_next_element();
  assert_compiled_template(text = "@gmail.com", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_dollar_prefixed_with_backslash_is_a_literal_dollar(void)
{
  cfg_set_version(configuration, 0x304);
  assert_template_compile("Test \\$STRING");
  assert_compiled_template(text = "Test $STRING", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);

  cfg_set_version(configuration, 0x305);
  assert_template_compile("Test \\$STRING");
  assert_compiled_template(text = "Test \\", default_value = NULL, value_handle = log_msg_get_value_handle("STRING"),
                           type = LTE_VALUE, msg_ref = 0);
}

static void
test_colon_dash_in_braces_is_parsed_as_default_value(void)
{
  assert_template_compile("${MESSAGE:-default value}");
  assert_compiled_template(text = "", default_value = "default value", macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);

  assert_template_compile("${MESSAGE:-}");
  assert_compiled_template(text = "", default_value = "", macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_double_dollars_is_a_literal_dollar(void)
{
  assert_template_compile("$$VALUE_NAME");
  assert_compiled_template(text = "$VALUE_NAME", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);

  assert_template_compile("$${VALUE_NAME}");
  assert_compiled_template(text = "${VALUE_NAME}", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_dollar_with_an_invalid_macro_name_without_braces_is_parsed_as_a_literal_dollar(void)
{
  assert_template_compile("$:VALUE_NAME");
  assert_compiled_template(text = "$:VALUE_NAME", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);

  assert_template_compile("$");
  assert_compiled_template(text = "$", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);

}

static void
test_backslash_without_finishing_the_escape_sequence_is_ignored(void)
{
  cfg_set_version(configuration, 0x304);
  assert_template_compile("foo\\");
  assert_compiled_template(text = "foo", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);

  cfg_set_version(configuration, 0x305);
  assert_template_compile("foo\\");
  assert_compiled_template(text = "foo\\", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_double_at_is_a_literal_at(void)
{
  assert_template_compile("${MESSAGE}@@12");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);

  select_next_element();
  assert_compiled_template(text = "@12", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_template_compile_macro(void)
{
  TEMPLATE_TESTCASE(test_simple_string_literal);
  TEMPLATE_TESTCASE(test_simple_macro);
  TEMPLATE_TESTCASE(test_macro_and_text);
  TEMPLATE_TESTCASE(test_macro_without_braces);
  TEMPLATE_TESTCASE(test_macro_name_without_braces_are_terminated_with_non_identifier_characters);
  TEMPLATE_TESTCASE(test_macro_without_at_records_that_no_msgref_was_present_by_msgref_zero);
  TEMPLATE_TESTCASE(test_macro_with_at_references_a_single_msg_in_the_context_stack_by_setting_msgref);
  TEMPLATE_TESTCASE(test_macro_with_invalid_msgref_are_recognized_as_the_top_element_in_the_stack);
  TEMPLATE_TESTCASE(test_dollar_prefixed_with_backslash_is_a_literal_dollar);
  TEMPLATE_TESTCASE(test_colon_dash_in_braces_is_parsed_as_default_value);
  TEMPLATE_TESTCASE(test_double_dollars_is_a_literal_dollar);
  TEMPLATE_TESTCASE(test_dollar_with_an_invalid_macro_name_without_braces_is_parsed_as_a_literal_dollar);
  TEMPLATE_TESTCASE(test_backslash_without_finishing_the_escape_sequence_is_ignored);
  TEMPLATE_TESTCASE(test_double_at_is_a_literal_at);
}

static void
test_simple_value(void)
{
  assert_template_compile("${VALUE_NAME}");
  assert_compiled_template(text = "", default_value = NULL, value_handle = log_msg_get_value_handle("VALUE_NAME"),
                           type = LTE_VALUE, msg_ref = 0);
}

static void
test_value_without_braces(void)
{
  assert_template_compile("$VALUE_NAME");
  assert_compiled_template(text = "", default_value = NULL, value_handle = log_msg_get_value_handle("VALUE_NAME"),
                           type = LTE_VALUE, msg_ref = 0);
}

static void
test_backslash_within_braces_is_taken_literally(void)
{
  assert_template_compile("${VALUE\\}NAME}");
  assert_compiled_template(text = "", default_value = NULL, value_handle = log_msg_get_value_handle("VALUE\\"),
                           type = LTE_VALUE, msg_ref = 0);
}

static void
test_value_name_can_be_the_empty_string_when_referenced_using_braces(void)
{
  assert_template_compile("${}");
  assert_compiled_template(text = "", default_value = NULL, value_handle = log_msg_get_value_handle(""), type = LTE_VALUE,
                           msg_ref = 0);
}

static void
test_template_compile_value(void)
{
  TEMPLATE_TESTCASE(test_simple_value);
  TEMPLATE_TESTCASE(test_value_without_braces);
  TEMPLATE_TESTCASE(test_backslash_within_braces_is_taken_literally);
  TEMPLATE_TESTCASE(test_value_name_can_be_the_empty_string_when_referenced_using_braces);
}

static void
test_simple_template_function(void)
{
  assert_template_compile("$(hello)");
  assert_compiled_template(text = "", default_value = NULL, func.ops = get_template_function_ops("hello"),
                           type = LTE_FUNC, msg_ref = 0);
}

static void
test_complicated_template_function(void)
{
  assert_template_compile("$( hello \\tes\t\t\t value(xyz) \"value with spaces\" 'test value with spa\"ces')@2");
  assert_compiled_template(text = "", default_value = NULL, func.ops = get_template_function_ops("hello"),
                           type = LTE_FUNC, msg_ref = 3);
}

static void
test_simple_template_function_with_additional_text(void)
{
  assert_template_compile("$(hello)test value");
  assert_compiled_template(text = "", default_value = NULL, func.ops = get_template_function_ops("hello"),
                           type = LTE_FUNC, msg_ref = 0);

  select_next_element();
  assert_compiled_template(text = "test value", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_qouted_string_in_name_template_function(void)
{
  assert_template_compile("$(he\"ll\"o)");
  assert_compiled_template(text = "", default_value = NULL, func.ops = get_template_function_ops("hello"),
                           type = LTE_FUNC, msg_ref = 0);
}

static void
test_template_compile_func(void)
{
  TEMPLATE_TESTCASE(test_simple_template_function);
  TEMPLATE_TESTCASE(test_complicated_template_function);
  TEMPLATE_TESTCASE(test_simple_template_function_with_additional_text);
  TEMPLATE_TESTCASE(test_qouted_string_in_name_template_function);
}

static void
test_invalid_macro(void)
{
  assert_failed_template_compile("${MESSAGE", "Invalid macro, '}' is missing, error_pos='9'");
  assert_compiled_template(text = "error in template: ${MESSAGE", default_value = NULL, macro = M_NONE, type = LTE_MACRO,
                           msg_ref = 0);
}

static void
test_invalid_subst(void)
{
  assert_failed_template_compile("${MESSAGE:1}", "Unknown substitution function, error_pos='10'");
  assert_compiled_template(text = "error in template: ${MESSAGE:1}", default_value = NULL, macro = M_NONE,
                           type = LTE_MACRO, msg_ref = 0);
}

static void
test_template_function_bad1(void)
{
  assert_failed_template_compile("$( hello \\tes\t\t\t value(xyz \"value with spaces\" 'test value with spa\"ces')",
                                 "Invalid template function reference, missing function name or imbalanced '(', error_pos='73'");
  assert_compiled_template(text =
                           "error in template: $( hello \\tes\t\t\t value(xyz \"value with spaces\" 'test value with spa\"ces')",
                           default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_template_function_bad2(void)
{
  assert_failed_template_compile("$( hello \\tes\t\t\t value xyz \"value with spaces\" 'test value with spa\"ces'",
                                 "Invalid template function reference, missing function name or imbalanced '(', error_pos='72'");
  assert_compiled_template(text =
                           "error in template: $( hello \\tes\t\t\t value xyz \"value with spaces\" 'test value with spa\"ces'",
                           default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_template_function_bad3(void)
{
  assert_failed_template_compile("$(hello \"This is an unclosed quoted string)",
                                 "Invalid template function reference, missing function name or imbalanced '(', error_pos='8'");
  assert_compiled_template(text = "error in template: $(hello \"This is an unclosed quoted string)", default_value = NULL,
                           macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

static void
test_unknown_function(void)
{
  assert_failed_template_compile("$(unknown function)", "Unknown template function \"unknown\"");
  assert_compiled_template(text = "error in template: $(unknown function)", default_value = NULL, macro = M_NONE,
                           type = LTE_MACRO, msg_ref = 0);
}

static void
test_template_compile_negativ_tests(void)
{
  TEMPLATE_TESTCASE(test_invalid_macro);
  TEMPLATE_TESTCASE(test_invalid_subst);
  TEMPLATE_TESTCASE(test_template_function_bad1);
  TEMPLATE_TESTCASE(test_template_function_bad2);
  TEMPLATE_TESTCASE(test_template_function_bad3);
  TEMPLATE_TESTCASE(test_unknown_function);
}

int main(int argc, char **argv)
{
  msg_init(FALSE);

  configuration = cfg_new_snippet();
  log_msg_registry_init();
  log_template_global_init();
  plugin_register(&configuration->plugin_context, &hello_plugin, 1);

  test_template_compile_macro();
  test_template_compile_value();
  test_template_compile_func();
  test_template_compile_negativ_tests();

  log_msg_registry_deinit();
  msg_deinit();
  return 0;
}
