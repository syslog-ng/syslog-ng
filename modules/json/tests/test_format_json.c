/*
 * Copyright (c) 2011-2018 Balabit
 * Copyright (c) 2011-2013 Gergely Nagy <algernon@balabit.hu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <criterion/criterion.h>
#include "libtest/cr_template.h"

#include "apphook.h"
#include "plugin.h"
#include "cfg.h"
#include "logmsg/logmsg.h"

void
setup(void)
{
  app_startup();
  setenv("TZ", "UTC", TRUE);
  tzset();
  init_template_tests();
  cfg_load_module(configuration, "json-plugin");
}

void
teardown(void)
{
  deinit_template_tests();
  app_shutdown();
}

TestSuite(format_json, .init = setup, .fini = teardown);


Test(format_json, test_format_json)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  assert_template_format("$(format-json MSG=$MSG)", "{\"MSG\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format("$(format-json MSG=$escaping)",
                         "{\"MSG\":\"binary stuff follows \\\"\\\\xad árvíztűrőtükörfúrógép\"}");
  assert_template_format("$(format-json MSG=$escaping2)", "{\"MSG\":\"\\\\xc3\"}");
  assert_template_format("$(format-json MSG=$null)", "{\"MSG\":\"binary\\\\x00stuff\"}");
  assert_template_format_with_context("$(format-json MSG=$MSG)",
                                      "{\"MSG\":\"árvíztűrőtükörfúrógép\"}{\"MSG\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format("$(format-json --scope rfc3164)",
                         "{\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 10:34:56\"}");
  assert_template_format("$(format-json msg.text=$MSG msg.id=42 host=bzorp)",
                         "{\"msg\":{\"text\":\"árvíztűrőtükörfúrógép\",\"id\":\"42\"},\"host\":\"bzorp\"}");
  assert_template_format("$(format-json msg.text.str=$MSG msg.text.len=42 msg.id=42 host=bzorp)",
                         "{\"msg\":{\"text\":{\"str\":\"árvíztűrőtükörfúrógép\",\"len\":\"42\"},\"id\":\"42\"},\"host\":\"bzorp\"}");
  assert_template_format("$(format-json kernel.SUBSYSTEM=pci kernel.DEVICE.type=pci kernel.DEVICE.name=0000:02:00.0 MSGID=801 MESSAGE=test)",
                         "{\"kernel\":{\"SUBSYSTEM\":\"pci\",\"DEVICE\":{\"type\":\"pci\",\"name\":\"0000:02:00.0\"}},\"MSGID\":\"801\",\"MESSAGE\":\"test\"}");
  assert_template_format("$(format-json .foo=bar)", "{\"_foo\":\"bar\"}");
  assert_template_format("$(format-json --scope rfc3164,rfc3164)",
                         "{\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 10:34:56\"}");
  assert_template_format("$(format-json sdata.win@18372.4.fruit=\"pear\" sdata.win@18372.4.taste=\"good\")",
                         "{\"sdata\":{\"win@18372.4\":{\"taste\":\"good\",\"fruit\":\"pear\"}}}");


  assert_template_format("$(format-json --scope selected_macros)",
                         "{\"TAGS\":[\"alma\",\"korte\",\"citrom\",\"tag,containing,comma\"],\"SOURCEIP\":\"10.11.12.13\",\"SEQNUM\":\"999\",\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 10:34:56\"}");

  assert_template_format("$(format-json --scope rfc3164 --key *.*)",
                         "{\"_unix\":{\"uid\":\"1000\",\"gid\":\"1000\",\"cmd\":\"command\"},\"_json\":{\"sub\":{\"value2\":\"subvalue2\",\"value1\":\"subvalue1\"},\"foo\":\"bar\"},\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 10:34:56\",\"APP\":{\"VALUE7\":\"value\",\"VALUE6\":\"value\",\"VALUE5\":\"value\",\"VALUE4\":\"value\",\"VALUE3\":\"value\",\"VALUE2\":\"value\",\"VALUE\":\"value\",\"STRIP5\":\"\",\"STRIP4\":\"value\",\"STRIP3\":\"     value     \",\"STRIP2\":\"value     \",\"STRIP1\":\"     value\",\"QVALUE\":\"\\\"value\\\"\"}}");

  assert_template_format("$(format-json --scope syslog-proto)",
                         "{\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 10:34:56\"}");

  assert_template_format("$(format-json @program=${PROGRAM})", "{\"@program\":\"syslog-ng\"}");
  assert_template_format("$(format-json @program.123=${PROGRAM})", "{\"@program\":{\"123\":\"syslog-ng\"}}");
  assert_template_format("$(format-json .@program.123=${PROGRAM})", "{\"_@program\":{\"123\":\"syslog-ng\"}}");
  assert_template_format("$(format-json @.program=${PROGRAM})", "{\"@\":{\"program\":\"syslog-ng\"}}");
  assert_template_format("$(format-json .program.n@me=${PROGRAM})", "{\"_program\":{\"n@me\":\"syslog-ng\"}}");
  assert_template_format("$(format-json .program.@name=${PROGRAM})", "{\"_program\":{\"@name\":\"syslog-ng\"}}");
  assert_template_format("$(format-json --leave-initial-dot .program.@name=${PROGRAM})",
                         "{\".program\":{\"@name\":\"syslog-ng\"}}");
  assert_template_format("$(format-json --leave-initial-dot .program.@name=${PROGRAM} .program.foo .program.bar)",
                         "{\".program\":{\"@name\":\"syslog-ng\"}}");
  assert_template_format("$(format-json .program.@name=${PROGRAM} .program.foo .program.bar --key .program.* --shift-levels 2 --add-prefix _)",
                         "{\"_@name\":\"syslog-ng\"}");

  cfg_set_version_without_validation(configuration, VERSION_VALUE_3_38);
  assert_template_format("$(format-json --scope selected_macros)",
                         "{\"TAGS\":\"alma,korte,citrom,\\\"tag,containing,comma\\\"\",\"SOURCEIP\":\"10.11.12.13\",\"SEQNUM\":\"999\",\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 10:34:56\"}");

}

Test(format_json, test_format_json_key)
{
  assert_template_format("$(format-json --key PID)", "{\"PID\":\"23323\"}");
  assert_template_format("$(format-json --key HOST)", "{\"HOST\":\"bzorp\"}");
  assert_template_format("$(format-json --key MESSAGE)", "{\"MESSAGE\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format("$(format-json --key HOST --key MESSAGE)",
                         "{\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\"}");

  assert_template_format("$(format-json --scope selected-macros --key MSG --exclude TAGS)",
                         "{\"SOURCEIP\":\"10.11.12.13\",\"SEQNUM\":\"999\",\"PROGRAM\":\"syslog-ng\",\"PRIORITY\":\"err\",\"PID\":\"23323\",\"MSG\":\"árvíztűrőtükörfúrógép\",\"MESSAGE\":\"árvíztűrőtükörfúrógép\",\"HOST\":\"bzorp\",\"FACILITY\":\"local3\",\"DATE\":\"Feb 11 10:34:56\"}");

  assert_template_format("$(format-json --key MSG)", "{\"MSG\":\"árvíztűrőtükörfúrógép\"}");
  assert_template_format("$(format-json --key DATE)", "{\"DATE\":\"Feb 11 10:34:56\"}");
  assert_template_format("$(format-json --key PRI)", "{\"PRI\":\"155\"}");
}

Test(format_json, test_format_json_rekey)
{
  assert_template_format("$(format-json .msg.text=dotted --rekey .* --shift 1 --add-prefix _)",
                         "{\"_msg\":{\"text\":\"dotted\"}}");
}

Test(format_json, test_format_json_omit_empty_values)
{
  assert_template_format("$(format-json --omit-empty-values msg.set=value msg.unset='')",
                         "{\"msg\":{\"set\":\"value\"}}");

  assert_template_format("$(format-json --omit-empty-values msg.set=value --key empty_value)",
                         "{\"msg\":{\"set\":\"value\"}}");
  assert_template_format("$(format-json msg.set=value --key empty_value)",
                         "{\"msg\":{\"set\":\"value\"},\"empty_value\":\"\"}");
}

Test(format_json, test_format_json_with_type_hints)
{
  assert_template_format("$(format-json i32=int32(1234))",
                         "{\"i32\":1234}");
  assert_template_format("$(format-json \"i=ifoo(\")",
                         "{\"i\":\"ifoo(\"}");
  assert_template_format("$(format-json b=boolean(TRUE))",
                         "{\"b\":true}");
  assert_template_format("$(format-json null=null())",
                         "{\"null\":null}");
  assert_template_format("$(format-json null=null(whatever))",
                         "{\"null\":null}");
  assert_template_format("$(format-json null=null($DATE))",
                         "{\"null\":null}");
  assert_template_format("$(format-json l=list($comma_value))",
                         "{\"l\":[\"value\",\"with\",\"a\",\"comma\"]}");
  assert_template_format("$(format-json b=literal(whatever))",
                         "{\"b\":whatever}");
  assert_template_format("$(format-json b=literal($(format-json subkey=bar)))",
                         "{\"b\":{\"subkey\":\"bar\"}}");

}

Test(format_json, test_v3x_value_pairs_yields_string_values)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_3_38);
  /* in 3.x mode, numbers remain strings */

  /* template */
  assert_template_format("$(format-json foo=$number1)",
                         "{\"foo\":\"123\"}");

  /* name value pair */
  assert_template_format("$(format-json number1)",
                         "{\"number1\":\"123\"}");

  /* macro */
  assert_template_format("$(format-json FACILITY_NUM)",
                         "{\"FACILITY_NUM\":\"19\"}");


  /* auto-cast is the same but suppresses warning */

  /* template */
  assert_template_format("$(format-json --auto-cast foo=$number1)",
                         "{\"foo\":\"123\"}");

  /* name value pair */
  assert_template_format("$(format-json --auto-cast number1)",
                         "{\"number1\":\"123\"}");

  /* macro */
  assert_template_format("$(format-json --auto-cast FACILITY_NUM)",
                         "{\"FACILITY_NUM\":\"19\"}");
}

Test(format_json, test_v40_value_pairs_yields_typed_values)
{
  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);

  /* in 4.x mode, numbers become numbers */

  /* template */
  assert_template_format("$(format-json foo=$number1)",
                         "{\"foo\":123}");

  /* name value pair */
  assert_template_format("$(format-json number1)",
                         "{\"number1\":123}");

  /* macro */
  assert_template_format("$(format-json FACILITY_NUM)",
                         "{\"FACILITY_NUM\":19}");

  /* auto-cast is the same but suppresses warning */

  /* template */
  assert_template_format("$(format-json --auto-cast foo=$number1)",
                         "{\"foo\":123}");

  /* name value pair */
  assert_template_format("$(format-json --auto-cast number1)",
                         "{\"number1\":123}");

  /* macro */
  assert_template_format("$(format-json --auto-cast FACILITY_NUM)",
                         "{\"FACILITY_NUM\":19}");

  /* RFC8259 number sanitization */
  assert_template_format("$(format-json num=int(+00014))", "{\"num\":14}");
  assert_template_format("$(format-json num=int(014))", "{\"num\":14}");
  assert_template_format("$(format-json num=int(000014))", "{\"num\":14}");
  assert_template_format("$(format-json num=int(+0x0014))", "{\"num\":20}");
  assert_template_format("$(format-json num=int(0x14))", "{\"num\":20}");
  assert_template_format("$(format-json num=int(-0x14))", "{\"num\":-20}");
  assert_template_format("$(format-json num=int(0x00014))", "{\"num\":20}");
  assert_template_format("$(format-json num=int(+14))", "{\"num\":14}");
  assert_template_format("$(format-json num=int(-14))", "{\"num\":-14}");
  assert_template_format("$(format-json num=int(-021))", "{\"num\":-21}");
  assert_template_format("$(format-json num=int(+9223372036854775804))", "{\"num\":9223372036854775804}");
}

Test(format_json, test_cast_option_always_yields_strings_regardless_of_versions)
{
  /* template */
  assert_template_format("$(format-json --cast foo=$number1)",
                         "{\"foo\":\"123\"}");

  /* name value pair */
  assert_template_format("$(format-json --cast number1)",
                         "{\"number1\":\"123\"}");

  /* macro */
  assert_template_format("$(format-json --cast FACILITY_NUM)",
                         "{\"FACILITY_NUM\":\"19\"}");

  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  /* unless forced to be strings */

  /* template */
  assert_template_format("$(format-json --cast foo=$number1)",
                         "{\"foo\":\"123\"}");

  /* name value pair */
  assert_template_format("$(format-json --cast number1)",
                         "{\"number1\":\"123\"}");

  /* macro */
  assert_template_format("$(format-json --cast FACILITY_NUM)",
                         "{\"FACILITY_NUM\":\"19\"}");
}

Test(format_json, test_no_cast_option_always_yields_types_regardless_of_versions)
{
  /* template */
  assert_template_format("$(format-json --no-cast foo=$number1)",
                         "{\"foo\":123}");

  /* name value pair */
  assert_template_format("$(format-json --no-cast number1)",
                         "{\"number1\":123}");

  /* macro */
  assert_template_format("$(format-json --no-cast FACILITY_NUM)",
                         "{\"FACILITY_NUM\":19}");

  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);
  /* unless forced to be strings */

  /* template */
  assert_template_format("$(format-json --no-cast foo=$number1)",
                         "{\"foo\":123}");

  /* name value pair */
  assert_template_format("$(format-json --no-cast number1)",
                         "{\"number1\":123}");

  /* macro */
  assert_template_format("$(format-json --no-cast FACILITY_NUM)",
                         "{\"FACILITY_NUM\":19}");
}

Test(format_json, test_format_json_on_error)
{
  configuration->template_options.on_error = ON_ERROR_DROP_MESSAGE | ON_ERROR_SILENT;
  assert_template_format("$(format-json x=y bad=boolean(blah) foo=bar)",
                         "");
  assert_template_format("$(format-json x=y bad=boolean($unsetvalue) foo=bar)",
                         "");
  assert_template_format("$(format-json x=y bad=int32(blah) foo=bar)",
                         "");
  assert_template_format("$(format-json x=y bad=int32($unsetvalue) foo=bar)",
                         "");
  assert_template_format("$(format-json x=y bad=int64(blah) foo=bar)",
                         "");
  assert_template_format("$(format-json x=y bad=int64($unsetvalue) foo=bar)",
                         "");
  assert_template_format("$(format-json x=y bad=double(blah) foo=bar)",
                         "");
  assert_template_format("$(format-json x=y bad=double($unsetvalue) foo=bar)",
                         "");

  configuration->template_options.on_error = ON_ERROR_DROP_PROPERTY | ON_ERROR_SILENT;
  assert_template_format("$(format-json x=y bad=boolean(blah) foo=bar)",
                         "{\"x\":\"y\",\"foo\":\"bar\"}");
  assert_template_format("$(format-json x=y bad=boolean(blah))",
                         "{\"x\":\"y\"}");
  assert_template_format("$(format-json x=y bad=int32(blah))",
                         "{\"x\":\"y\"}");
  assert_template_format("$(format-json x=y bad=int32($unsetvalue))",
                         "{\"x\":\"y\"}");
  assert_template_format("$(format-json x=y bad=int64(blah))",
                         "{\"x\":\"y\"}");
  assert_template_format("$(format-json x=y z=boolean(blah))",
                         "{\"x\":\"y\"}");

  configuration->template_options.on_error = ON_ERROR_FALLBACK_TO_STRING | ON_ERROR_SILENT;
  assert_template_format("$(format-json x=y bad=boolean(blah) foo=bar)",
                         "{\"x\":\"y\",\"foo\":\"bar\",\"bad\":\"blah\"}");
  assert_template_format("$(format-json x=y bad=boolean(blah))",
                         "{\"x\":\"y\",\"bad\":\"blah\"}");
  assert_template_format("$(format-json x=y bad=int32(blah))",
                         "{\"x\":\"y\",\"bad\":\"blah\"}");
  assert_template_format("$(format-json x=y bad=int32($unsetvalue))",
                         "{\"x\":\"y\",\"bad\":\"\"}");
  assert_template_format("$(format-json x=y bad=int64(blah))",
                         "{\"x\":\"y\",\"bad\":\"blah\"}");

}

Test(format_json, test_format_json_with_utf8)
{
  LogMessage *msg = create_empty_message();
  log_msg_set_value_by_name(msg, "UTF8-C2", "\xc2\xbf \xc2\xb6 \xc2\xa9 \xc2\xb1", -1); // ¿ ¶ © ±
  log_msg_set_value_by_name(msg, "UTF8-C3", "\xc3\x88 \xc3\x90", -1); // È Ð
  log_msg_set_value_by_name(msg, "UTF8-CTRL", "\x07\x09", -1);

  assert_template_format_msg("$(format-json MSG=\"${UTF8-C2}\")", "{\"MSG\":\"\xc2\xbf \xc2\xb6 \xc2\xa9 \xc2\xb1\"}",
                             msg);
  assert_template_format_msg("$(format-json MSG=\"${UTF8-C3}\")", "{\"MSG\":\"\xc3\x88 \xc3\x90\"}", msg);

  assert_template_format_msg("$(format-json MSG=\"${UTF8-CTRL}\")", "{\"MSG\":\"\\u0007\\t\"}", msg);

  log_msg_unref(msg);
}

Test(format_json, test_format_json_with_bytes)
{
  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name_with_type(msg, "bytes", "\0\1\2\3", 4, LM_VT_BYTES);
  log_msg_set_value_by_name_with_type(msg, "protobuf", "\4\5\6\7", 4, LM_VT_PROTOBUF);

  cfg_set_version_without_validation(configuration, VERSION_VALUE_4_0);

  assert_template_format_msg("$(format-json --scope nv-pairs)", "{}", msg);
  assert_template_format_msg("$(format-json --include-bytes --scope nv-pairs)",
                             "{\"protobuf\":\"BAUGBw==\",\"bytes\":\"AAECAw==\"}", msg);

  cfg_set_version_without_validation(configuration, VERSION_VALUE_3_38);

  /*
   * format-json() receives the bytes value with string type, so it does not know, that it has to
   * base64 encode it.  This scenario is extremely rare and illogical, as the bytes type was introduced
   * in v4.3 and no one should try to use it with v3 config.
   */
  assert_template_format_msg("$(format-json --scope nv-pairs)", "{}", msg);
  assert_template_format_msg("$(format-json --include-bytes --scope nv-pairs)",
                             "{\"protobuf\":\"\\u0004\\u0005\\u0006\\u0007\","
                             "\"bytes\":\"\\\\x00\\u0001\\u0002\\u0003\"}", msg);

  log_msg_unref(msg);
}

Test(format_json, test_format_flat_json)
{
  LogMessage *msg = create_empty_message();

  assert_template_format_msg("$(format-flat-json a.b.c1=abc a.b.d=abd a.bc=abc)",
                             "{\"a.bc\":\"abc\",\"a.b.d\":\"abd\",\"a.b.c1\":\"abc\"}",
                             msg);

  log_msg_unref(msg);
}

Test(format_json, test_format_flat_json_direct)
{
  LogMessage *msg = create_empty_message();

  assert_template_format_msg("$(format-flat-json a=b c=d)",
                             "{\"c\":\"d\",\"a\":\"b\"}",
                             msg);

  log_msg_unref(msg);
}

Test(format_json, test_format_flat_json_with_type_hints)
{
  assert_template_format("$(format-flat-json i32=int32(1234))",
                         "{\"i32\":1234}");
  assert_template_format("$(format-flat-json \"i=ifoo(\")",
                         "{\"i\":\"ifoo(\"}");
  assert_template_format("$(format-flat-json b=boolean(TRUE))",
                         "{\"b\":true}");
  assert_template_format("$(format-flat-json l=list($comma_value))",
                         "{\"l\":[\"value\",\"with\",\"a\",\"comma\"]}");
  assert_template_format("$(format-flat-json b=literal(whatever))",
                         "{\"b\":whatever}");
  assert_template_format("$(format-flat-json b=literal($(format-flat-json subkey=bar)))",
                         "{\"b\":{\"subkey\":\"bar\"}}");
}

Test(format_json, test_format_json_performance)
{
  perftest_template("$(format-json APP.*)\n");
  perftest_template("$(format-flat-json APP.*)\n");
  perftest_template("<$PRI>1 $ISODATE $LOGHOST @syslog-ng - - ${SDATA:--} $(format-json --scope all-nv-pairs "
                    "--exclude 0* --exclude 1* --exclude 2* --exclude 3* --exclude 4* --exclude 5* "
                    "--exclude 6* --exclude 7* --exclude 8* --exclude 9* "
                    "--exclude SOURCE "
                    "--exclude .SDATA.* "
                    "..RSTAMP='${R_UNIXTIME}${R_TZ}' "
                    "..TAGS=${TAGS})\n");
  perftest_template("<$PRI>1 $ISODATE $LOGHOST @syslog-ng - - ${SDATA:--} $(format-json --leave-initial-dot --scope all-nv-pairs "
                    "--exclude 0* --exclude 1* --exclude 2* --exclude 3* --exclude 4* --exclude 5* "
                    "--exclude 6* --exclude 7* --exclude 8* --exclude 9* "
                    "--exclude SOURCE "
                    "--exclude .SDATA.* "
                    "..RSTAMP='${R_UNIXTIME}${R_TZ}' "
                    "..TAGS=${TAGS})\n");
}

Test(format_json, test_format_json_with_key_delimiter)
{
  assert_template_format("$(format-json --key-delimiter \"\t\" \".foo\t.b.a.r.\"=\"baz\")",
                         "{\".foo\":{\".b.a.r.\":\"baz\"}}");
  assert_template_format("$(format-json --key-delimiter \".\" \".foo.bar\"=\"baz\")",
                         "{\"_foo\":{\"bar\":\"baz\"}}");
  assert_template_format("$(format-json \".foo.bar\"=\"baz\")",
                         "{\"_foo\":{\"bar\":\"baz\"}}");

  assert_template_format("$(format-json --key-delimiter ~ top~foo=1 top~bar=2 top~baz=3 top~sub~key1=val1 top~sub~key2=val2)",
                         "{\"top\":{\"sub\":{\"key2\":\"val2\",\"key1\":\"val1\"},\"foo\":\"1\",\"baz\":\"3\",\"bar\":\"2\"}}");
}
