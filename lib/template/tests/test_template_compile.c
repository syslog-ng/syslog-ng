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
#include "apphook.h"
#include "grab-logging.h"
#include <criterion/criterion.h>
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
    cr_assert_str_eq(current_elem->text, expected.text, "%s", "Bad compiled template text"); \
    if (expected.default_value) \
      { \
        cr_assert_str_eq(current_elem->default_value, expected.default_value, "%s", "Bad compiled template default value"); \
      } \
    else \
      { \
        cr_assert_null(current_elem->default_value, "%s", "Bad compiled template default value"); \
      } \
    cr_assert_eq(current_elem->msg_ref, expected.msg_ref, "%s", "Bad compiled template msg_ref");

#define fill_expected_template_element(element, text, default_value, spec, type, msg_ref)  {\
    element.text; \
    element.default_value; \
    element.spec; \
    element.type;  \
    element.msg_ref; }

#define assert_compiled_template(set_text, set_default_value, spec, type, msg_ref) \
  do { \
    const gchar *text; \
    set_text; \
    gchar *text_mut = g_strdup(text); \
    \
    const gchar *default_value; \
    set_default_value; \
    gchar *default_value_mut = g_strdup(default_value); \
    \
    LogTemplateElem expected_elem;                                \
                                                                                                                                                                \
    fill_expected_template_element(expected_elem, text = text_mut, \
                                   default_value = default_value_mut, spec, type, msg_ref); \
    cr_assert_eq((current_elem->type), (expected_elem.type), "%s", "Bad compiled template type");               \
    assert_common_element(expected_elem);                               \
    if ((expected_elem.type) == LTE_MACRO) cr_assert_eq(current_elem->macro, expected_elem.macro, "%s", "Bad compiled template macro");     \
    if ((expected_elem.type) == LTE_VALUE) cr_assert_eq(current_elem->value_handle, expected_elem.value_handle, "%s", "Bad compiled template macro"); \
    if ((expected_elem.type) == LTE_FUNC) cr_assert_eq(current_elem->func.ops, expected_elem.func.ops, "%s", "Bad compiled template macro");  \
    g_free(text_mut); \
    g_free(default_value_mut); \
} while (0)


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

  cr_assert(log_template_compile(template, template_string, &error), "%s", "Can't compile template");
  cr_assert_str_eq(template->template, template_string, "%s", "Bad stored template");
  select_first_element();
}

static inline void
assert_failed_template_compile(const gchar *template_string, const gchar *expected_error_message)
{
  GError *error = NULL;

  cr_assert_not(log_template_compile(template, template_string, &error), "%s", "Can compile bad template");
  cr_assert_str_eq(error->message, expected_error_message, "%s", "Bad error message");

  g_clear_error(&error);
  select_first_element();
}

static gpointer
get_template_function_ops(const gchar *name)
{
  Plugin *plugin;

  plugin = cfg_find_plugin(configuration, LL_CONTEXT_TEMPLATE_FUNC, name);
  cr_assert_not_null(plugin, "Template function %s is not found", name);

  if (plugin)
    return plugin->construct(plugin);
  return NULL;
}

Test(template_compile, test_simple_string_literal)
{
  const gchar *text_ = "Test String";

  assert_template_compile(text_);
  assert_compiled_template(text = text_, default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_simple_macro)
{
  assert_template_compile("${MESSAGE}");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_macro_and_text)
{

  assert_template_compile("${MESSAGE}test value");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);

  select_next_element();
  assert_compiled_template(text = "test value", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_macro_without_braces)
{
  assert_template_compile("$MESSAGE");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_macro_name_without_braces_are_terminated_with_non_identifier_characters)
{
  /* macro names consist of [A-Z0-9_] */
  assert_template_compile("$MESSAGE test value");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);

  select_next_element();
  assert_compiled_template(text = " test value", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_macro_without_at_records_that_no_msgref_was_present_by_msgref_zero)
{
  assert_template_compile("${MESSAGE}");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_macro_with_at_references_a_single_msg_in_the_context_stack_by_setting_msgref)
{
  assert_template_compile("${MESSAGE}@0");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 1);

  assert_template_compile("${MESSAGE}@1");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 2);

}

Test(template_compile, test_macro_with_invalid_msgref_are_recognized_as_the_top_element_in_the_stack)
{
  assert_template_compile("${MESSAGE}@gmail.com");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);

  select_next_element();
  assert_compiled_template(text = "@gmail.com", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_dollar_prefixed_with_backslash_is_a_literal_dollar)
{
  cfg_set_version(configuration, 0x304);
  assert_template_compile("Test \\$STRING");
  assert_compiled_template(text = "Test $STRING", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);

  cfg_set_version(configuration, 0x305);
  assert_template_compile("Test \\$STRING");
  assert_compiled_template(text = "Test \\", default_value = NULL, value_handle = log_msg_get_value_handle("STRING"),
                           type = LTE_VALUE, msg_ref = 0);
}

Test(template_compile, test_colon_dash_in_braces_is_parsed_as_default_value)
{
  assert_template_compile("${MESSAGE:-default value}");
  assert_compiled_template(text = "", default_value = "default value", macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);

  assert_template_compile("${MESSAGE:-}");
  assert_compiled_template(text = "", default_value = "", macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_double_dollars_is_a_literal_dollar)
{
  assert_template_compile("$$VALUE_NAME");
  assert_compiled_template(text = "$VALUE_NAME", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);

  assert_template_compile("$${VALUE_NAME}");
  assert_compiled_template(text = "${VALUE_NAME}", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_dollar_with_an_invalid_macro_name_without_braces_is_parsed_as_a_literal_dollar)
{
  assert_template_compile("$:VALUE_NAME");
  assert_compiled_template(text = "$:VALUE_NAME", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);

  assert_template_compile("$");
  assert_compiled_template(text = "$", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);

}

Test(template_compile, test_backslash_without_finishing_the_escape_sequence_is_ignored)
{
  cfg_set_version(configuration, 0x304);
  assert_template_compile("foo\\");
  assert_compiled_template(text = "foo", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);

  cfg_set_version(configuration, 0x305);
  assert_template_compile("foo\\");
  assert_compiled_template(text = "foo\\", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_double_at_is_a_literal_at)
{
  assert_template_compile("${MESSAGE}@@12");
  assert_compiled_template(text = "", default_value = NULL, macro = M_MESSAGE, type = LTE_MACRO, msg_ref = 0);

  select_next_element();
  assert_compiled_template(text = "@12", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_simple_value)
{
  assert_template_compile("${VALUE_NAME}");
  assert_compiled_template(text = "", default_value = NULL, value_handle = log_msg_get_value_handle("VALUE_NAME"),
                           type = LTE_VALUE, msg_ref = 0);
}

Test(template_compile, test_value_without_braces)
{
  assert_template_compile("$VALUE_NAME");
  assert_compiled_template(text = "", default_value = NULL, value_handle = log_msg_get_value_handle("VALUE_NAME"),
                           type = LTE_VALUE, msg_ref = 0);
}

Test(template_compile, test_backslash_within_braces_is_taken_literally)
{
  assert_template_compile("${VALUE\\}NAME}");
  assert_compiled_template(text = "", default_value = NULL, value_handle = log_msg_get_value_handle("VALUE\\"),
                           type = LTE_VALUE, msg_ref = 0);
}

Test(template_compile, test_value_name_can_be_the_empty_string_when_referenced_using_braces)
{
  assert_template_compile("${}");
  assert_compiled_template(text = "", default_value = NULL, value_handle = log_msg_get_value_handle(""), type = LTE_VALUE,
                           msg_ref = 0);
}

Test(template_compile, test_simple_template_function)
{
  assert_template_compile("$(hello)");
  assert_compiled_template(text = "", default_value = NULL, func.ops = get_template_function_ops("hello"),
                           type = LTE_FUNC, msg_ref = 0);
}

Test(template_compile, test_complicated_template_function)
{
  assert_template_compile("$( hello \\tes\t\t\t value(xyz) \"value with spaces\" 'test value with spa\"ces')@2");
  assert_compiled_template(text = "", default_value = NULL, func.ops = get_template_function_ops("hello"),
                           type = LTE_FUNC, msg_ref = 3);
}

Test(template_compile, test_simple_template_function_with_additional_text)
{
  assert_template_compile("$(hello)test value");
  assert_compiled_template(text = "", default_value = NULL, func.ops = get_template_function_ops("hello"),
                           type = LTE_FUNC, msg_ref = 0);

  select_next_element();
  assert_compiled_template(text = "test value", default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_qouted_string_in_name_template_function)
{
  assert_template_compile("$(he\"ll\"o)");
  assert_compiled_template(text = "", default_value = NULL, func.ops = get_template_function_ops("hello"),
                           type = LTE_FUNC, msg_ref = 0);
}

Test(template_compile, test_invalid_macro)
{
  assert_failed_template_compile("${MESSAGE", "Invalid macro, '}' is missing, error_pos='9'");
  assert_compiled_template(text = "error in template: ${MESSAGE", default_value = NULL, macro = M_NONE, type = LTE_MACRO,
                           msg_ref = 0);
}

Test(template_compile, test_invalid_subst)
{
  assert_failed_template_compile("${MESSAGE:1}", "Unknown substitution function, error_pos='10'");
  assert_compiled_template(text = "error in template: ${MESSAGE:1}", default_value = NULL, macro = M_NONE,
                           type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_template_function_bad1)
{
  assert_failed_template_compile("$( hello \\tes\t\t\t value(xyz \"value with spaces\" 'test value with spa\"ces')",
                                 "Invalid template function reference, missing function name or imbalanced '(', error_pos='73'");
  assert_compiled_template(text =
                             "error in template: $( hello \\tes\t\t\t value(xyz \"value with spaces\" 'test value with spa\"ces')",
                           default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_template_function_bad2)
{
  assert_failed_template_compile("$( hello \\tes\t\t\t value xyz \"value with spaces\" 'test value with spa\"ces'",
                                 "Invalid template function reference, missing function name or imbalanced '(', error_pos='72'");
  assert_compiled_template(text =
                             "error in template: $( hello \\tes\t\t\t value xyz \"value with spaces\" 'test value with spa\"ces'",
                           default_value = NULL, macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_template_function_bad3)
{
  assert_failed_template_compile("$(hello \"This is an unclosed quoted string)",
                                 "Invalid template function reference, missing function name or imbalanced '(', error_pos='8'");
  assert_compiled_template(text = "error in template: $(hello \"This is an unclosed quoted string)", default_value = NULL,
                           macro = M_NONE, type = LTE_MACRO, msg_ref = 0);
}

Test(template_compile, test_unknown_function)
{
  assert_failed_template_compile("$(unknown function)", "Unknown template function \"unknown\"");
  assert_compiled_template(text = "error in template: $(unknown function)", default_value = NULL, macro = M_NONE,
                           type = LTE_MACRO, msg_ref = 0);
}

static void
setup(void)
{
  msg_init(FALSE);

  configuration = cfg_new_snippet();
  log_msg_registry_init();
  log_template_global_init();
  plugin_register(&configuration->plugin_context, &hello_plugin, 1);

  template = log_template_new(configuration, NULL);
  start_grabbing_messages();
}

static void
teardown(void)
{
  log_msg_registry_deinit();
  msg_deinit();

  log_template_unref(template);
  template = NULL;

  stop_grabbing_messages();
}

TestSuite(template_compile, .init = setup, .fini = teardown);
