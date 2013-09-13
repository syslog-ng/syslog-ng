#include "template_lib.h"
#include "apphook.h"
#include "plugin.h"

void
test_cond_funcs(void)
{
  assert_template_format_with_context("$(grep 'facility(local3)' $PID)", "23323,23323");
  assert_template_format_with_context("$(grep -m 1 'facility(local3)' $PID)", "23323");
  assert_template_format_with_context("$(grep 'facility(local3)' $PID $PROGRAM)", "23323,syslog-ng,23323,syslog-ng");
  assert_template_format_with_context("$(grep 'facility(local4)' $PID)", "");
  assert_template_format_with_context("$(grep ('$FACILITY' == 'local4') $PID)", "");
  assert_template_format_with_context("$(grep ('$FACILITY(' == 'local3(') $PID)", "23323,23323");
  assert_template_format_with_context("$(grep ('$FACILITY(' == 'local4)') $PID)", "");
  assert_template_format_with_context("$(grep \\'$FACILITY\\'\\ ==\\ \\'local4\\' $PID)", "");

  assert_template_format_with_context("$(if 'facility(local4)' alma korte)", "korte");
  assert_template_format_with_context("$(if 'facility(local3)' alma korte)", "alma");

  assert_template_format_with_context("$(if '\"$FACILITY\" lt \"local3\"' alma korte)", "korte");
  assert_template_format_with_context("$(if '\"$FACILITY\" le \"local3\"' alma korte)", "alma");
  assert_template_format_with_context("$(if '\"$FACILITY\" eq \"local3\"' alma korte)", "alma");
  assert_template_format_with_context("$(if '\"$FACILITY\" ne \"local3\"' alma korte)", "korte");
  assert_template_format_with_context("$(if '\"$FACILITY\" gt \"local3\"' alma korte)", "korte");
  assert_template_format_with_context("$(if '\"$FACILITY\" ge \"local3\"' alma korte)", "alma");

  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" < \"19\"' alma korte)", "korte");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" <= \"19\"' alma korte)", "alma");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" == \"19\"' alma korte)", "alma");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" != \"19\"' alma korte)", "korte");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" > \"19\"' alma korte)", "korte");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" >= \"19\"' alma korte)", "alma");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" >= \"19\" and \"kicsi\" == \"nagy\"' alma korte)", "korte");
  assert_template_format_with_context("$(if '\"$FACILITY_NUM\" >= \"19\" or \"kicsi\" == \"nagy\"' alma korte)", "alma");

  assert_template_format_with_context("$(grep 'facility(local3)' $PID)@0", "23323");
  assert_template_format_with_context("$(grep 'facility(local3)' $PID)@1", "23323");
  assert_template_format_with_context("$(grep 'facility(local3)' $PID)@2", "");
}

void
test_str_funcs(void)
{
  assert_template_format("$(ipv4-to-int $SOURCEIP)", "168496141");

  assert_template_format("$(length $HOST $PID)", "5 5");
  assert_template_format("$(length $HOST)", "5");
  assert_template_format("$(length)", "");

  assert_template_format("$(substr $HOST 1 3)", "zor");
  assert_template_format("$(substr $HOST 1)", "zorp");
  assert_template_format("$(substr $HOST -1)", "p");
  assert_template_format("$(substr $HOST -2 1)", "r");

  assert_template_format("$(strip ${APP.STRIP1})", "value");
  assert_template_format("$(strip ${APP.STRIP2})", "value");
  assert_template_format("$(strip ${APP.STRIP3})", "value");
  assert_template_format("$(strip ${APP.STRIP4})", "value");
  assert_template_format("$(strip ${APP.STRIP5})", "");

  assert_template_format("$(strip ${APP.STRIP1} ${APP.STRIP2} ${APP.STRIP3} ${APP.STRIP4} ${APP.STRIP5})", "value value value value ");

  assert_template_format("$(sanitize alma/bela)", "alma_bela");
  assert_template_format("$(sanitize -r @ alma/bela)", "alma@bela");
  assert_template_format("$(sanitize -i @ alma@bela)", "alma_bela");
  assert_template_format("$(sanitize -i '@/l ' alma@/bela)", "a_ma__be_a");
  assert_template_format("$(sanitize alma\x1b_bela)", "alma__bela");
  assert_template_format("$(sanitize -C alma\x1b_bela)", "alma\x1b_bela");

  assert_template_format("$(sanitize $HOST $PROGRAM)", "bzorp/syslog-ng");

  assert_template_format("$(indent-multi-line 'foo\nbar')", "foo\n\tbar");

  assert_template_format("$(lowercase ŐRÜLT ÍRÓ)", "őrült író");
  assert_template_format("$(uppercase őrült író)", "ŐRÜLT ÍRÓ");

  assert_template_format("$(delimit \"\t\" \",\" \"hello\tworld\")", "hello,world");
}

void
test_numeric_funcs(void)
{
  assert_template_format("$(+ $FACILITY_NUM 1)", "20");
  assert_template_format("$(+ -1 -1)", "-2");
  assert_template_format("$(- $FACILITY_NUM 1)", "18");
  assert_template_format("$(- $FACILITY_NUM 20)", "-1");
  assert_template_format("$(* $FACILITY_NUM 2)", "38");
  assert_template_format("$(/ $FACILITY_NUM 2)", "9");
  assert_template_format("$(% $FACILITY_NUM 3)", "1");
  assert_template_format("$(/ $FACILITY_NUM 0)", "NaN");
  assert_template_format("$(% $FACILITY_NUM 0)", "NaN");
  assert_template_format("$(+ foo bar)", "NaN");
}

void
test_misc_funcs(void)
{
  unsetenv("OHHELLO");
  setenv("TEST_ENV", "test-env", 1);

  assert_template_format("$(env OHHELLO)", "");
  assert_template_format("$(env TEST_ENV)", "test-env");
}

int
main(int argc G_GNUC_UNUSED, char *argv[] G_GNUC_UNUSED)
{
  app_startup();
  init_template_tests();
  plugin_load_module("basicfuncs", configuration, NULL);

  test_cond_funcs();
  test_str_funcs();
  test_numeric_funcs();
  test_misc_funcs();

  deinit_template_tests();
  app_shutdown();
}
