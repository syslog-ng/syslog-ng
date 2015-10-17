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

int
main(int argc, char *argv[])
{
  test_string_array_to_list_converts_to_an_equivalent_glist();
}
