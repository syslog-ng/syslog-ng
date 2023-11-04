#include <criterion/criterion.h>
#include "filterx/filterx-object.h"
#include "filterx/object-primitive.h"
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

Test(filterx_object, test_filterx_object_construction_and_free)
{
  FilterXObject *fobj = filterx_object_new(&FILTERX_TYPE_NAME(object));

  filterx_object_unref(fobj);
}

Test(filterx_object, test_filterx_primitive_int_marshales_to_integer_repr)
{
  FilterXObject *fobj = filterx_integer_new(36);
  assert_marshaled_object(fobj, "36", LM_VT_INTEGER);
  filterx_object_unref(fobj);
}

Test(filterx_object, test_filterx_primitive_int_is_mapped_to_a_json_int)
{
  FilterXObject *fobj = filterx_integer_new(36);
  assert_object_json_equals(fobj, "36");
  filterx_object_unref(fobj);
}

Test(filterx_object, test_filterx_primitive_int_is_truthy_if_nonzero)
{
  FilterXObject *fobj = filterx_integer_new(36);
  cr_assert(filterx_object_truthy(fobj));
  cr_assert_not(filterx_object_falsy(fobj));
  filterx_object_unref(fobj);

  fobj = filterx_integer_new(0);
  cr_assert_not(filterx_object_truthy(fobj));
  cr_assert(filterx_object_falsy(fobj));
  filterx_object_unref(fobj);
}

Test(filterx_object, test_filterx_primitive_double_marshales_to_double_repr)
{
  FilterXObject *fobj = filterx_double_new(36.07);
  assert_marshaled_object(fobj, "36.07", LM_VT_DOUBLE);
  filterx_object_unref(fobj);
}

Test(filterx_object, test_filterx_primitive_double_is_mapped_to_a_json_float)
{
  FilterXObject *fobj = filterx_double_new(36.0);
  assert_object_json_equals(fobj, "36.0");
  filterx_object_unref(fobj);
}

Test(filterx_object, test_filterx_primitive_double_is_truthy_if_nonzero)
{
  FilterXObject *fobj = filterx_double_new(1);
  cr_assert(filterx_object_truthy(fobj));
  cr_assert_not(filterx_object_falsy(fobj));
  filterx_object_unref(fobj);

  fobj = filterx_double_new(0.0);
  cr_assert_not(filterx_object_truthy(fobj));
  cr_assert(filterx_object_falsy(fobj));
  filterx_object_unref(fobj);
}

Test(filterx_object, test_filterx_primitive_bool_marshales_to_bool_repr)
{
  FilterXObject *fobj = filterx_boolean_new(TRUE);
  assert_marshaled_object(fobj, "true", LM_VT_BOOLEAN);
  filterx_object_unref(fobj);
}

Test(filterx_object, test_filterx_primitive_bool_is_mapped_to_a_json_bool)
{
  FilterXObject *fobj = filterx_boolean_new(TRUE);
  assert_object_json_equals(fobj, "true");
  filterx_object_unref(fobj);

  fobj = filterx_boolean_new(FALSE);
  assert_object_json_equals(fobj, "false");
  filterx_object_unref(fobj);
}

Test(filterx_object, test_filterx_primitive_bool_is_truthy_if_true)
{
  FilterXObject *fobj = filterx_boolean_new(TRUE);
  cr_assert(filterx_object_truthy(fobj));
  cr_assert_not(filterx_object_falsy(fobj));
  filterx_object_unref(fobj);

  fobj = filterx_boolean_new(0.0);
  cr_assert_not(filterx_object_truthy(fobj));
  cr_assert(filterx_object_falsy(fobj));
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

TestSuite(filterx_object, .init = setup, .fini = teardown);
