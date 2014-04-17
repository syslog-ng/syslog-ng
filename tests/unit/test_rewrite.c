#include "apphook.h"
#include "plugin.h"
#include "testutils.h"
#include "config_parse_lib.h"
#include "logrewrite.h"
#include "test_case.h"

typedef struct _RewriteTestCase {
  TestCase super;
  LogProcessRule *log_process_rule;
  LogRewrite *test_rewrite;
  LogMessage *msg;
  NVHandle device_id;
  LogPathOptions po;
} RewriteTestCase;

void
rewrite_testcase_setup(TestCase *s)
{
  RewriteTestCase *self = (RewriteTestCase *)s;
  configuration = cfg_new(0x500);
  assert_true(parse_config("rewrite s_test{set(\"00100\", value(\"device_id\") condition(program(\"ARCGIS\")));};", LL_CONTEXT_ROOT, NULL, NULL), ASSERTION_ERROR("Parsing the given configuration failed"));
  self->log_process_rule = g_hash_table_lookup(configuration->rewriters, "s_test");
  assert_not_null(self->log_process_rule, ASSERTION_ERROR("Can't find parsed rewrite rule"));
  self->device_id = log_msg_get_value_handle("device_id");
  self->test_rewrite = (LogRewrite *)self->log_process_rule->head->data;
  self->msg = log_msg_new_empty();
  log_msg_set_value(self->msg, LM_V_PROGRAM, "ARCGIS", -1);
  self->po.ack_needed = FALSE;
  self->po.flow_control_requested = FALSE;
  self->po.matched = NULL;
}

void
rewrite_testcase_teardown(TestCase *s)
{
  RewriteTestCase *self = (RewriteTestCase *)s;
  log_msg_unref(self->msg);
  cfg_free(configuration);
}

void
test_condition_success(TestCase *s)
{
  RewriteTestCase *self = (RewriteTestCase *) s;
  log_pipe_queue((LogPipe *) self->test_rewrite, log_msg_ref(self->msg), &self->po);
  assert_string(log_msg_get_value(self->msg, self->device_id, NULL), "00100", ASSERTION_ERROR("Bad device_id"));
}

void
test_reference_on_condition_cloned(TestCase *s)
{
  RewriteTestCase *self = (RewriteTestCase *) s;
  LogPipe *cloned_rule = log_process_pipe_clone(&self->test_rewrite->super);
  assert_guint32(self->test_rewrite->condition->ref_cnt, 2, ASSERTION_ERROR("Bad reference number of condition"));
  log_pipe_unref(cloned_rule);
  assert_guint32(self->test_rewrite->condition->ref_cnt, 1, ASSERTION_ERROR("Bad reference number of condition"));
}

int
main(int argc, char **argv)
{
	app_startup();
	putenv("TZ=MET-1METDST");
	tzset();

	start_grabbing_messages();
	RUN_TESTCASE(RewriteTestCase, rewrite_testcase, test_condition_success);
	RUN_TESTCASE(RewriteTestCase, rewrite_testcase, test_reference_on_condition_cloned);
	stop_grabbing_messages();
}
