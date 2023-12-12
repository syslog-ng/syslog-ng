#include <criterion/criterion.h>
#include "filterx/object-message-value.h"
#include "apphook.h"

static void
assert_marshaled_object(FilterXObject *obj, const gchar *repr, LogMessageValueType type)
{
  GString *b = g_string_sized_new(0);
  LogMessageValueType t;

  filterx_object_marshal(obj, b, &t);
  cr_assert_str_eq(b->str, repr);
  cr_assert_eq(t, type);
  g_string_free(b, TRUE);
}

static void
assert_object_json_equals(FilterXObject *obj, const gchar *expected_json_repr)
{
  struct json_object *jso = NULL;

  cr_assert(filterx_object_map_to_json(obj, &jso) == TRUE, "error mapping to json, expected json was: %s", expected_json_repr);
  const gchar *json_repr = json_object_to_json_string_ext(jso, JSON_C_TO_STRING_PLAIN);
  cr_assert_str_eq(json_repr, expected_json_repr);
  json_object_put(jso);
}

Test(filterx_message, test_filterx_object_message_marshals_to_the_stored_values)
{
  FilterXObject *fobj = filterx_message_value_new("True", 4, LM_VT_BOOLEAN);
  assert_marshaled_object(fobj, "True", LM_VT_BOOLEAN);
  filterx_object_unref(fobj);

  fobj = filterx_message_value_new("42", 2, LM_VT_INTEGER);
  assert_marshaled_object(fobj, "42", LM_VT_INTEGER);
  filterx_object_unref(fobj);

  fobj = filterx_message_value_new_ref(g_strdup("35.6"), 4, LM_VT_DOUBLE);
  assert_marshaled_object(fobj, "35.6", LM_VT_DOUBLE);
  filterx_object_unref(fobj);

  gchar borrowed_value[] = "string";

  fobj = filterx_message_value_new_borrowed(borrowed_value, -1, LM_VT_STRING);
  assert_marshaled_object(fobj, "string", LM_VT_STRING);
  borrowed_value[0]++;
  assert_marshaled_object(fobj, "ttring", LM_VT_STRING);
  filterx_object_unref(fobj);
}

Test(filterx_message, test_filterx_object_value_maps_to_the_right_json_value)
{
  FilterXObject *fobj = filterx_message_value_new("True", 4, LM_VT_BOOLEAN);
  assert_object_json_equals(fobj, "true");
  filterx_object_unref(fobj);

  fobj = filterx_message_value_new("42", 2, LM_VT_INTEGER);
  assert_object_json_equals(fobj, "42");
  filterx_object_unref(fobj);

  fobj = filterx_message_value_new_ref(g_strdup("36.0"), 4, LM_VT_DOUBLE);
  assert_object_json_equals(fobj, "36.0");
  filterx_object_unref(fobj);

  gchar borrowed_value[] = "string";

  fobj = filterx_message_value_new_borrowed(borrowed_value, -1, LM_VT_STRING);
  assert_object_json_equals(fobj, "\"string\"");
  borrowed_value[0]++;
  assert_object_json_equals(fobj, "\"ttring\"");
  filterx_object_unref(fobj);
}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  app_shutdown();
}

TestSuite(filterx_message, .init = setup, .fini = teardown);
