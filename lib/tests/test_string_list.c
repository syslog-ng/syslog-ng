#include "misc.h"
#include "testutils.h"

static void
test_string_array_to_list_converts_to_an_equivalent_glist(void)
{
  const gchar *arr[] = {
    "foo",
    "bar",
    "baz",
    NULL
  };
  GList *l;

  l = string_array_to_list(arr);
  assert_gint(g_list_length(l), 3, "converted list is not the expected length");
  assert_string(l->data, "foo", "first element is expected to be foo");
  assert_string(l->next->data, "bar", "second element is expected to be bar");
  assert_string(l->next->next->data, "baz", "third element is expected to be baz");
  string_list_free(l);
}

static void
test_clone_string_array_duplicates_elements_while_leaving_token_values_intact(void)
{
  const gchar *arr[] = {
    "foo",
    "bar",
    "baz",
    NULL
  };
  GList *l, *l2;

  l = string_array_to_list(arr);
  l = g_list_append(l, GUINT_TO_POINTER(1));
  l = g_list_append(l, GUINT_TO_POINTER(2));
  l2 = string_list_clone(l);
  string_list_free(l);

  assert_gint(g_list_length(l2), 5, "converted list is not the expected length");
  assert_string(l2->data, "foo", "first element is expected to be foo");
  assert_string(l2->next->data, "bar", "second element is expected to be bar");
  assert_string(l2->next->next->data, "baz", "third element is expected to be baz");
  assert_gint(GPOINTER_TO_UINT(l2->next->next->next->data), 1, "fourth element is expected to be a token, with a value of 1");
  assert_gint(GPOINTER_TO_UINT(l2->next->next->next->next->data), 2, "fifth element is expected to be a token, with a value of 2");

  string_list_free(l2);
}

int
main(int argc, char *argv[])
{
  test_string_array_to_list_converts_to_an_equivalent_glist();
  test_clone_string_array_duplicates_elements_while_leaving_token_values_intact();
}
