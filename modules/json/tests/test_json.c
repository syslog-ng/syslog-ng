#include "template_lib.h"
#include "apphook.h"
#include "plugin.h"
#include "cfg.h"

void
test_format_json(void)
{
  assert_template_format("$(format-json MSG=$MSG)", "{\"MSG\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format_with_context("$(format-json MSG=$MSG)", "{\"MSG\":\"árvíztűrőtükörfúrógép\"}{\"MSG\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format("$(format-json --scope rfc3164)", "{\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 10:34:56\"}");
  assert_template_format("$(format-json msg.text=$MSG msg.id=42 host=bzorp)", "{\"msg\":{\"text\":\"árvíztűrőtükörfúrógép\",\"id\":\"42\"},\"host\":\"bzorp\"}");
  assert_template_format("$(format-json msg.text.str=$MSG msg.text.len=42 msg.id=42 host=bzorp)",
                         "{\"msg\":{\"text\":{\"str\":\"árvíztűrőtükörfúrógép\",\"len\":\"42\"},\"id\":\"42\"},\"host\":\"bzorp\"}");
  assert_template_format("$(format-json kernel.SUBSYSTEM=pci kernel.DEVICE.type=pci kernel.DEVICE.name=0000:02:00.0 MSGID=801 MESSAGE=test)",
                         "{\"kernel\":{\"SUBSYSTEM\":\"pci\",\"DEVICE\":{\"type\":\"pci\",\"name\":\"0000:02:00.0\"}},\"MSGID\":\"801\",\"MESSAGE\":\"test\"}");
  assert_template_format("$(format-json .foo=bar)", "{\"_foo\":\"bar\"}");
  assert_template_format("$(format-json --scope rfc3164,rfc3164)", "{\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 10:34:56\"}");
  assert_template_format("$(format-json sdata.win@18372.4.fruit=\"pear\" sdata.win@18372.4.taste=\"good\")",
                         "{\"sdata\":{\"win@18372.4\":{\"taste\":\"good\",\"fruit\":\"pear\"}}}");

  assert_template_format("$(format-json --scope selected_macros)", "{\"TAGS\":\"alma,korte,citrom\",\"SOURCEIP\":\"10.11.12.13\",\"SEQNUM\":\"999\",\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 10:34:56\"}");

  assert_template_format("$(format-json --scope everything --exclude=SYSUPTIME --exclude=LOGHOST --exclude=C_YEAR_DAY --exclude=C_YEAR --exclude=C_WEEK_DAY_NAME --exclude=C_WEEK_DAY_ABBREV --exclude=C_WEEK_DAY --exclude=C_WEEKDAY --exclude=C_WEEK --exclude=C_UNIXTIME --exclude=C_TZOFFSET --exclude=C_TZ --exclude=C_STAMP --exclude=C_SEC --exclude=C_MONTH_WEEK --exclude=C_MONTH_NAME --exclude=C_MONTH_ABBREV --exclude=C_MONTH --exclude=C_MIN --exclude=C_ISODATE --exclude=C_HOUR --exclude=C_FULLDATE --exclude=C_DAY --exclude=C_DATE)",
                         "{\"_unix\":{\"uid\":\"1000\",\"gid\":\"1000\",\"cmd\":\"command\"},\"_json\":{\"sub\":{\"value2\":\"subvalue2\",\"value1\":\"subvalue1\"},\"foo\":\"bar\"},\"YEAR_DAY\":\"042\",\"YEAR\":\"2006\",\"WEEK_DAY_NAME\":\"Saturday\",\"WEEK_DAY_ABBREV\":\"Sat\",\"WEEK_DAY\":\"7\",\"WEEKDAY\":\"Sat\",\"WEEK\":\"06\",\"USEC\":\"000000\",\"UNIXTIME\":\"1139650496\",\"TZOFFSET\":\"+01:00\",\"TZ\":\"+01:00\",\"TAGS\":\"alma,korte,citrom\",\"TAG\":\"9b\",\"S_YEAR_DAY\":\"042\",\"S_YEAR\":\"2006\",\"S_WEEK_DAY_NAME\":\"Saturday\",\"S_WEEK_DAY_ABBREV\":\"Sat\",\"S_WEEK_DAY\":\"7\",\"S_WEEKDAY\":\"Sat\",\"S_WEEK\":\"06\",\"S_USEC\":\"000000\",\"S_UNIXTIME\":\"1139650496\",\"S_TZOFFSET\":\"+01:00\",\"S_TZ\":\"+01:00\",\"S_STAMP\":\"Feb 11 10:34:56\",\"S_SEC\":\"56\",\"S_MSEC\":\"000\",\"S_MONTH_WEEK\":\"1\",\"S_MONTH_NAME\":\"February\",\"S_MONTH_ABBREV\":\"Feb\",\"S_MONTH\":\"02\",\"S_MIN\":\"34\",\"S_ISODATE\":\"2006-02-11T10:34:56+01:00\",\"S_HOUR12\":\"10\",\"S_HOUR\":\"10\",\"S_FULLDATE\":\"2006 Feb 11 10:34:56\",\"S_DAY\":\"11\",\"S_DATE\":\"Feb 11 10:34:56\",\"S_AMPM\":\"AM\",\"STAMP\":\"Feb 11 10:34:56\",\"SOURCEIP\":\"10.11.12.13\",\"SEQNUM\":\"999\",\"SEC\":\"56\",\"SDATA\":\"[meta sequenceId=\\\"999\\\"]\",\"R_YEAR_DAY\":\"042\",\"R_YEAR\":\"2006\",\"R_WEEK_DAY_NAME\":\"Saturday\",\"R_WEEK_DAY_ABBREV\":\"Sat\",\"R_WEEK_DAY\":\"7\",\"R_WEEKDAY\":\"Sat\",\"R_WEEK\":\"06\",\"R_USEC\":\"639000\",\"R_UNIXTIME\":\"1139684315\",\"R_TZOFFSET\":\"+00:00\",\"R_TZ\":\"+00:00\",\"R_STAMP\":\"Feb 11 18:58:35\",\"R_SEC\":\"35\",\"R_MSEC\":\"639\",\"R_MONTH_WEEK\":\"1\",\"R_MONTH_NAME\":\"February\",\"R_MONTH_ABBREV\":\"Feb\",\"R_MONTH\":\"02\",\"R_MIN\":\"58\",\"R_ISODATE\":\"2006-02-11T18:58:35+00:00\",\"R_HOUR12\":\"06\",\"R_HOUR\":\"18\",\"R_FULLDATE\":\"2006 Feb 11 18:58:35\",\"R_DAY\":\"11\",\"R_DATE\":\"Feb 11 18:58:35\",\"R_AMPM\":\"PM\",\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PRI\":\"155\",\"PID\":\"23323\",\"MSGHDR\":\"syslog-ng[23323]:\",\"MSG\":\"árvíztűrőtükörfúrógép\",\"MSEC\":\"000\",\"MONTH_WEEK\":\"1\",\"MONTH_NAME\":\"February\",\"MONTH_ABBREV\":\"Feb\",\"MONTH\":\"02\",\"MIN\":\"34\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"LEVEL_NUM\":\"3\",\"LEVEL\":\"err\",\"LEGACY_MSGHDR\":\"syslog-ng[23323]:\",\"ISODATE\":\"2006-02-11T10:34:56+01:00\",\"HOUR12\":\"10\",\"HOUR\":\"10\",\"HOST_FROM\":\"kismacska\",\"HOST\":\"bzorp\",\"FULLDATE\":\"2006 Feb 11 10:34:56\",\"FACILITY_NUM\":\"19\",\"FACILITY\":\"local3\",\"DAY\":\"11\",\"DATE\":\"Feb 11 10:34:56\",\"BSDTAG\":\"3T\",\"APP\":{\"VALUE\":\"value\",\"STRIP4\":\"value\",\"STRIP3\":\"     value     \",\"STRIP2\":\"value     \",\"STRIP1\":\"     value\",\"QVALUE\":\"\\\"value\\\"\"},\"AMPM\":\"AM\",\"1\":\"first-match\",\"0\":\"whole-match\"}");

  assert_template_format("$(format-json --scope rfc3164 --key *.*)",
                          "{\"_unix\":{\"uid\":\"1000\",\"gid\":\"1000\",\"cmd\":\"command\"},\"_json\":{\"sub\":{\"value2\":\"subvalue2\",\"value1\":\"subvalue1\"},\"foo\":\"bar\"},\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 10:34:56\",\"APP\":{\"VALUE\":\"value\",\"STRIP4\":\"value\",\"STRIP3\":\"     value     \",\"STRIP2\":\"value     \",\"STRIP1\":\"     value\",\"QVALUE\":\"\\\"value\\\"\"}}");


  assert_template_format("$(format-json --scope syslog-proto)", "{\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 10:34:56\"}");
}

void
test_format_json_key(void)
{
  assert_template_format("$(format-json --key PID)", "{\"PID\":\"23323\"}");
  assert_template_format("$(format-json --key HOST)", "{\"HOST\":\"bzorp\"}");
  assert_template_format("$(format-json --key MESSAGE)", "{\"MESSAGE\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format("$(format-json --key HOST --key MESSAGE)", "{\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\"}");

  assert_template_format("$(format-json --scope selected-macros --key MSG)", "{\"TAGS\":\"alma,korte,citrom\",\"SOURCEIP\":\"10.11.12.13\",\"SEQNUM\":\"999\",\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MSG\":\"árvíztűrőtükörfúrógép\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 10:34:56\"}");

  assert_template_format("$(format-json --key MSG)", "{\"MSG\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format("$(format-json --key DATE)", "{\"DATE\":\"Feb 11 10:34:56\"}");
  assert_template_format("$(format-json --key PRI)", "{\"PRI\":\"155\"}");
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
  test_format_json_key();
  test_format_json_rekey();
  test_format_json_with_type_hints();
  test_format_json_on_error();

  deinit_template_tests();
  app_shutdown();
}
