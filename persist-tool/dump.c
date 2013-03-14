#include "dump.h"
#include "json/json.h"

void
print_struct(gpointer data, gpointer user_data)
{
  PersistTool *self = (PersistTool *)user_data;
  gchar *name = (gchar *)data;
  StateHandler *handler;
  NameValueContainer *nv_pairs;

  handler = persist_tool_get_state_handler(self, name);
  nv_pairs = handler->dump_state(handler);

  fprintf(stdout, "%s = %s\n\n", (char *) data, name_value_container_get_json_string(nv_pairs));

  name_value_container_free(nv_pairs);
  state_handler_free(handler);
}

gint
dump_main(int argc, char *argv[])
{
  gint result = 0;
  PersistTool *self;
  GList *keys;

  if (argc < 2)
    {
      fprintf(stderr, "Persist file is a required parameter\n");
      return 1;
    }

  if (!g_file_test(argv[1], G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS))
    {
      fprintf(stderr, "Persist file doesn't exist; file = %s\n", argv[1]);
      return 1;
    }

  self = persist_tool_new(argv[1], persist_mode_dump);
  if (!self)
    {
      return 1;
    }

  keys = persist_state_get_key_list(self->state);

  g_list_foreach(keys, print_struct, self);
  g_list_free(keys);

  persist_tool_free(self);
  return result;
}
