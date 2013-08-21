#include "templates.c"
#include "logmsg.h"
#include "testutils.h"
#include "cfg.h"
#include "plugin.h"

#define SIMPLE_STRING "Test String"
#define SIMPLE_MACRO "${MESSAGE}"
#define SIMPLE_MACRO_UNBRACE "$MESSAGE"
#define SIMPLE_MACRO_UNBRACE_ADD_TEXT "$MESSAGE test value"
#define SIMPLE_MACRO_MSGREF "${MESSAGE}@1"
#define SIMPLE_MACRO_INVALID_REF "${MESSAGE}@gmail.com"
#define SIMPLE_MACRO_ESCAPED_REF "${MESSAGE}@@12"
#define SIMPLE_MACRO_ADD_TEXT "${MESSAGE}test value"
#define ESCAPED_CHAR "Test \\$STRING"
#define VALID_SUBST "${MESSAGE:-default value}"
#define TRICKY_VALUE "$$VALUE_NAME"
#define TRICKY_VALUE_2 "$:VALUE_NAME"
#define TRICKY_VALUE_3 "$${VALUE_NAME}"
#define TRICKY_VALUE_4 "\\"
#define TRICKY_VALUE_5 "$"
#define TRICKY_VALUE_6 "${MESSAGE:-}"
#define TRICKY_VALUE_7 "${}"
#define TRICKY_VALUE_8 "\\tsomething ${MESSAGE}"

#define INVALID_MACRO "${MESSAGE"
#define INVALID_SUBST "${MESSAGE:1}"

#define SIMPLE_VALUE "${VALUE_NAME}"
#define SIMPLE_VALUE_UNBRACE "$VALUE_NAME"
#define SIMPLE_VALUE_ESCAPED "${VALUE\\}NAME}"

#define SIMPLE_TEMPLATE_FUNCTION "$(hello)"
#define SIMPLE_TEMPLATE_FUNCTION_WITH_ADDITIONAL_TEXT "$(hello)test value"
#define COMPLICATED_TEMPLATE_FUNCTION "$( hello \\tes\t\t\t value(xyz) \"value with spaces\" 'test value with spa\"ces')@2"
#define COMPLICATED_TEMPLATE_FUNCTION_BAD_1 "$( hello \\tes\t\t\t value(xyz \"value with spaces\" 'test value with spa\"ces')"
#define COMPLICATED_TEMPLATE_FUNCTION_BAD_2 "$( hello \\tes\t\t\t value xyz \"value with spaces\" 'test value with spa\"ces'"

#define SIMPLE_UNKNOWN_FUNCTION "$(unknown function)"

static void
test_internale_message(LogMessage *msg)
{
  log_msg_unref(msg);
  return;
}

static void
hello(LogMessage *msg, int argc, GString *argv[], GString *result)
{
  return;
}

TEMPLATE_FUNCTION_SIMPLE(hello);
Plugin hello_plugin = TEMPLATE_FUNCTION_PLUGIN(hello, "hello");
GlobalConfig *configuration;

#define assert_common_element(actual, expected) \
    assert_string(actual.text, expected.text, ASSERTION_ERROR("Bad compiled template text")); \
    if (expected.default_value) \
      { \
        assert_string(actual.default_value, expected.default_value, ASSERTION_ERROR("Bad compiled template default value")); \
      } \
    else \
      { \
        assert_gpointer(actual.default_value, NULL, ASSERTION_ERROR("Bad compiled template default value")); \
      } \
    assert_gint(actual.msg_ref, expected.msg_ref, ASSERTION_ERROR("Bad compiled template msg_ref"));

#define assert_value_element(actual, expected) \
    assert_gint(actual.type, LTE_VALUE, ASSERTION_ERROR("Bad compiled template type")); \
    assert_common_element(actual, expected); \
    assert_gint(actual.value_handle, expected.value_handle, ASSERTION_ERROR("Bad compiled template macro"));

#define assert_macro_element(actual, expected) \
    assert_gint(actual.type, LTE_MACRO, ASSERTION_ERROR("Bad compiled template type")); \
    assert_common_element(actual, expected); \
    assert_gint(actual.macro, expected.macro, ASSERTION_ERROR("Bad compiled template macro"));

#define assert_func_element(actual, expected) \
    assert_gint(actual.type, LTE_FUNC, ASSERTION_ERROR("Bad compiled template type")); \
    assert_common_element(actual, expected); \
    assert_gpointer(actual.func.ops, expected.func.ops, ASSERTION_ERROR("Bad compiled template macro"));

#define assert_bad_element(actual, expected, expected_error_message) \
    assert_string(err->message, expected_error_message, ASSERTION_ERROR("Bad error message")); \
    assert_macro_element(actual, expected)


#define assert_template_compile(template_string) \
    assert_true(log_template_compile(template, template_string, &err), ASSERTION_ERROR("Can't compile template")); \
    assert_string(template->template, template_string, ASSERTION_ERROR("Bad stored template"));

#define assert_failed_template_compile(template_string) assert_false(log_template_compile(template, template_string, &err), ASSERTION_ERROR("Can compile bad template"));



void
test_template_compile_macro()
{
  GError *err = NULL;
  LogTemplateElem *element;
  LogTemplateElem expected_element;
  LogTemplate *template = log_template_new(configuration, NULL);

  assert_template_compile(SIMPLE_STRING);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = SIMPLE_STRING, .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  assert_template_compile(SIMPLE_MACRO);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .macro = M_MESSAGE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  assert_template_compile(SIMPLE_MACRO_ADD_TEXT);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .macro = M_MESSAGE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);
  element = (LogTemplateElem *)template->compiled_template->next->data;
  expected_element = (LogTemplateElem){.text = "test value", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  assert_template_compile(SIMPLE_MACRO_UNBRACE);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .macro = M_MESSAGE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  assert_template_compile(SIMPLE_MACRO_UNBRACE_ADD_TEXT);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .macro = M_MESSAGE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);
  element = (LogTemplateElem *)template->compiled_template->next->data;
  expected_element = (LogTemplateElem){.text = " test value", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  assert_template_compile(SIMPLE_MACRO_MSGREF);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .macro = M_MESSAGE, .type = LTE_MACRO, .msg_ref = 2};
  assert_macro_element(element[0], expected_element);

  assert_template_compile(SIMPLE_MACRO_INVALID_REF);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .macro = M_MESSAGE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);
  element = (LogTemplateElem *)template->compiled_template->next->data;
  expected_element = (LogTemplateElem){.text = "@gmail.com", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  assert_template_compile(SIMPLE_MACRO_ESCAPED_REF);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .macro = M_MESSAGE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);
  element = (LogTemplateElem *)template->compiled_template->next->data;
  expected_element = (LogTemplateElem){.text = "@12", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  template->cfg->version = 0x402;
  assert_template_compile(ESCAPED_CHAR);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "Test $STRING", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  template->cfg->version = 0x500;
  assert_template_compile(ESCAPED_CHAR);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "Test \\", .default_value = NULL, .value_handle=log_msg_get_value_handle("STRING"), .type = LTE_VALUE, .msg_ref = 0};
  assert_value_element(element[0], expected_element);

  assert_template_compile(VALID_SUBST);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = "default value", .macro = M_MESSAGE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  assert_template_compile(TRICKY_VALUE);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "$VALUE_NAME", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  assert_template_compile(TRICKY_VALUE_2);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "$:VALUE_NAME", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  assert_template_compile(TRICKY_VALUE_3);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "${VALUE_NAME}", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  template->cfg->version = 0x402;
  assert_template_compile(TRICKY_VALUE_4);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  template->cfg->version = 0x500;
  assert_template_compile(TRICKY_VALUE_4);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "\\", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  assert_template_compile(TRICKY_VALUE_5);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "$", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  assert_template_compile(TRICKY_VALUE_6);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = "", .macro = M_MESSAGE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  assert_template_compile(TRICKY_VALUE_8);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "\\tsomething ", .default_value = NULL, .macro = M_MESSAGE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  log_template_unref(template);
}

void
test_template_compile_value()
{
  GError *err = NULL;
  LogTemplateElem *element;
  LogTemplateElem expected_element;
  LogTemplate *template = log_template_new(configuration, NULL);

  assert_template_compile(SIMPLE_VALUE);
  element = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .value_handle = log_msg_get_value_handle("VALUE_NAME"), .type = LTE_VALUE, .msg_ref = 0};
  assert_value_element(element[0], expected_element);

  assert_template_compile(SIMPLE_VALUE_UNBRACE);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .value_handle = log_msg_get_value_handle("VALUE_NAME"), .type = LTE_VALUE, .msg_ref = 0};
  assert_value_element(element[0], expected_element);

  assert_template_compile(SIMPLE_VALUE_ESCAPED);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .value_handle = log_msg_get_value_handle("VALUE\\"), .type = LTE_VALUE, .msg_ref = 0};
  assert_value_element(element[0], expected_element);

  assert_template_compile(TRICKY_VALUE_7);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .value_handle = log_msg_get_value_handle(""), .type = LTE_VALUE, .msg_ref = 0};
  assert_value_element(element[0], expected_element);

  log_template_unref(template);
}

void
test_template_compile_func()
{
  GError *err = NULL;
  LogTemplateElem *element;
  LogTemplateElem expected_element;
  LogTemplate *template = log_template_new(configuration, NULL);
  plugin_register(configuration, &hello_plugin, 1);

  assert_template_compile(SIMPLE_TEMPLATE_FUNCTION);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .func.ops = hello_construct(&hello_plugin, configuration, LL_CONTEXT_TEMPLATE_FUNC, "hello"), .type = LTE_FUNC, .msg_ref = 0};
  assert_func_element(element[0], expected_element);

  assert_template_compile(COMPLICATED_TEMPLATE_FUNCTION);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .func.ops = hello_construct(&hello_plugin, configuration, LL_CONTEXT_TEMPLATE_FUNC, "hello"), .type = LTE_FUNC, .msg_ref = 3};
  assert_func_element(element[0], expected_element);

  assert_template_compile(SIMPLE_TEMPLATE_FUNCTION_WITH_ADDITIONAL_TEXT);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "", .default_value = NULL, .func.ops = hello_construct(&hello_plugin, configuration, LL_CONTEXT_TEMPLATE_FUNC, "hello"), .type = LTE_FUNC, .msg_ref = 0};
  assert_func_element(element[0], expected_element);
  element = (LogTemplateElem *)template->compiled_template->next->data;
  expected_element = (LogTemplateElem){.text = "test value", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_macro_element(element[0], expected_element);

  log_template_unref(template);
}

void
test_template_compile_negativ_tests()
{
  GError *err = NULL;
  LogTemplateElem *element;
  LogTemplateElem expected_element;
  LogTemplate *template = log_template_new(configuration, NULL);

  assert_failed_template_compile(INVALID_MACRO);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "error in template: ${MESSAGE", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_bad_element(element[0], expected_element, "Invalid macro, '}' is missing, error_pos='9'");
  g_clear_error(&err);


  assert_failed_template_compile(INVALID_SUBST);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "error in template: ${MESSAGE:1}", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_bad_element(element[0], expected_element, "Unknown substitution function, error_pos='10'");
  g_clear_error(&err);


  assert_failed_template_compile(COMPLICATED_TEMPLATE_FUNCTION_BAD_1);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "error in template: $( hello \\tes\t\t\t value(xyz \"value with spaces\" 'test value with spa\"ces')", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_bad_element(element[0], expected_element, "Invalid template function reference, missing function name or inbalanced '(', error_pos='73'");
  g_clear_error(&err);

  assert_failed_template_compile(COMPLICATED_TEMPLATE_FUNCTION_BAD_2);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "error in template: $( hello \\tes\t\t\t value xyz \"value with spaces\" 'test value with spa\"ces'", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_bad_element(element[0], expected_element, "Invalid template function reference, missing function name or inbalanced '(', error_pos='72'");
  g_clear_error(&err);

  assert_failed_template_compile(SIMPLE_UNKNOWN_FUNCTION);
  element  = (LogTemplateElem *)template->compiled_template->data;
  expected_element = (LogTemplateElem){.text = "error in template: $(unknown function)", .default_value = NULL, .macro = M_NONE, .type = LTE_MACRO, .msg_ref = 0};
  assert_bad_element(element[0], expected_element, "Unknown template function unknown");
  g_clear_error(&err);

  log_template_unref(template);
}

int main(int argc, char **argv)
{
  configuration = cfg_new(0x500);
  log_msg_registry_init();
  log_template_global_init();
  msg_set_post_func(test_internale_message);

  test_template_compile_macro();
  test_template_compile_value();
  test_template_compile_func();
  test_template_compile_negativ_tests();

  log_msg_registry_deinit();
  return 0;
}
