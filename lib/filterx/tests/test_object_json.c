#include <criterion/criterion.h>
#include "filterx/object-json.h"
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

Test(filterx_json, test_filterx_object_json_marshals_to_the_stored_values)
{
  FilterXObject *fobj = construct_filterx_json_from_repr("{\"foo\": \"foovalue\"}", -1);
  assert_marshaled_object(fobj, "{\"foo\":\"foovalue\"}", LM_VT_JSON);
  filterx_object_unref(fobj);
}

Test(filterx_json, test_filterx_object_value_maps_to_the_right_json_value)
{
  FilterXObject *fobj = construct_filterx_json_from_repr("{\"foo\": \"foovalue\"}", -1);
  assert_object_json_equals(fobj, "{\"foo\":\"foovalue\"}");
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

TestSuite(filterx_json, .init = setup, .fini = teardown);
