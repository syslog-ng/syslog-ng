#include "template_lib.h"
#include "apphook.h"
#include "plugin.h"
#include "cfg.h"

void
test_format_json(void)
{
  assert_template_format("$(format-json MSG=$MSG)", "{\"MSG\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format_with_context("$(format-json MSG=$MSG)", "{\"MSG\":\"árvíztűrőtükörfúrógép\"}{\"MSG\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format("$(format-json --scope rfc3164)", "{\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 18:58:35\"}");
  assert_template_format("$(format-json msg.text=$MSG msg.id=42 host=bzorp)", "{\"msg\":{\"text\":\"árvíztűrőtükörfúrógép\",\"id\":\"42\"},\"host\":\"bzorp\"}");
  assert_template_format("$(format-json msg.text.str=$MSG msg.text.len=42 msg.id=42 host=bzorp)",
                         "{\"msg\":{\"text\":{\"str\":\"árvíztűrőtükörfúrógép\",\"len\":\"42\"},\"id\":\"42\"},\"host\":\"bzorp\"}");
  assert_template_format("$(format-json kernel.SUBSYSTEM=pci kernel.DEVICE.type=pci kernel.DEVICE.name=0000:02:00.0 MSGID=801 MESSAGE=test)",
                         "{\"kernel\":{\"SUBSYSTEM\":\"pci\",\"DEVICE\":{\"type\":\"pci\",\"name\":\"0000:02:00.0\"}},\"MSGID\":\"801\",\"MESSAGE\":\"test\"}");
  assert_template_format("$(format-json .foo=bar)", "{\"_foo\":\"bar\"}");
  assert_template_format("$(format-json --scope rfc3164,rfc3164)", "{\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 18:58:35\"}");
}

void
test_format_json_rekey(void)
{
  assert_template_format("$(format-json .msg.text=dotted --rekey .* --shift 1 --add-prefix _)",
                         "{\"_msg\":{\"text\":\"dotted\"}}");
}

void
test_format_json_with_type_hints(void)
{
  assert_template_format("$(format-json i32=int32(1234))",
                         "{\"i32\":1234}");
  assert_template_format("$(format-json \"i=ifoo(\")",
                         "{\"i\":\"ifoo(\"}");
  assert_template_format("$(format-json b=boolean(TRUE))",
                         "{\"b\":true}");
}

void
test_format_json_on_error(void)
{
  configuration->template_options.on_error = ON_ERROR_DROP_MESSAGE | ON_ERROR_SILENT;
  assert_template_format("$(format-json x=y bad=boolean(blah) foo=bar)",
                         "");
  assert_template_format("$(format-json x=y bad=int32(blah) foo=bar)",
                         "");
  assert_template_format("$(format-json x=y bad=int64(blah) foo=bar)",
                         "");

  configuration->template_options.on_error = ON_ERROR_DROP_PROPERTY | ON_ERROR_SILENT;
  assert_template_format("$(format-json x=y bad=boolean(blah) foo=bar)",
                         "{\"x\":\"y\",\"foo\":\"bar\"}");
  assert_template_format("$(format-json x=y bad=boolean(blah))",
                         "{\"x\":\"y\"}");
  assert_template_format("$(format-json x=y bad=int32(blah))",
                         "{\"x\":\"y\"}");
  assert_template_format("$(format-json x=y bad=int64(blah))",
                         "{\"x\":\"y\"}");

  configuration->template_options.on_error = ON_ERROR_FALLBACK_TO_STRING | ON_ERROR_SILENT;
  assert_template_format("$(format-json x=y bad=boolean(blah) foo=bar)",
                         "{\"x\":\"y\",\"foo\":\"bar\",\"bad\":\"blah\"}");
  assert_template_format("$(format-json x=y bad=boolean(blah))",
                         "{\"x\":\"y\",\"bad\":\"blah\"}");
  assert_template_format("$(format-json x=y bad=int32(blah))",
                         "{\"x\":\"y\",\"bad\":\"blah\"}");
  assert_template_format("$(format-json x=y bad=int64(blah))",
                         "{\"x\":\"y\",\"bad\":\"blah\"}");

}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();
  putenv("TZ=UTC");
  tzset();
  init_template_tests();
  plugin_load_module("json-plugin", configuration, NULL);

  test_format_json();
  test_format_json_rekey();
  test_format_json_with_type_hints();
  test_format_json_on_error();

  deinit_template_tests();
  app_shutdown();
}
