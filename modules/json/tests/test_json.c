#include "template_lib.h"
#include "apphook.h"
#include "plugin.h"

void
test_format_json(void)
{
  assert_template_format("$(format-json MSG=$MSG)", "{\"MSG\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format_with_context("$(format-json MSG=$MSG)", "{\"MSG\":\"árvíztűrőtükörfúrógép\"}{\"MSG\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format("$(format-json --scope rfc3164)", "{\"DATE\":\"Feb 11 19:58:35\",\"FACILITY\":\"local3\",\"HOST\":\"bzorp\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"PID\":\"23323\",\"PRIORITY\":\"err\",\"PROGRAM\":\"syslog-ng\"}");
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();
  init_template_tests();
  plugin_load_module("json-plugin", configuration, NULL);

  test_format_json();

  deinit_template_tests();
  app_shutdown();
}
