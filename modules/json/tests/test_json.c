#include "template_lib.h"
#include "apphook.h"
#include "plugin.h"

void
test_format_json(void)
{
  assert_template_format("$(format-json MSG=$MSG)", "{\"MSG\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format_with_context("$(format-json MSG=$MSG)", "{\"MSG\":\"árvíztűrőtükörfúrógép\"}{\"MSG\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format("$(format-json --scope rfc3164)", "{\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 19:58:35\"}");
  assert_template_format("$(format-json msg.text=$MSG msg.id=42 host=bzorp)", "{\"msg\":{\"text\":\"árvíztűrőtükörfúrógép\",\"id\":\"42\"},\"host\":\"bzorp\"}");
  assert_template_format("$(format-json msg.text.str=$MSG msg.text.len=42 msg.id=42 host=bzorp)",
                         "{\"msg\":{\"text\":{\"str\":\"árvíztűrőtükörfúrógép\",\"len\":\"42\"},\"id\":\"42\"},\"host\":\"bzorp\"}");
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();
  init_template_tests();
  plugin_load_module("json", configuration, NULL);

  test_format_json();

  deinit_template_tests();
  app_shutdown();
}
