#include "apphook.h"
#include "plugin.h"
#include "testutils.h"
#include "config_parse_lib.h"
#include "cfg-grammar.h"
#include "rewrite/rewrite-expr.h"

void
expect_config_parse_failure(const char *raw_rewrite_rule)
{
  char raw_config[1024];

  configuration = cfg_new(0x306);
  snprintf(raw_config, sizeof(raw_config), "rewrite s_test{ %s };", raw_rewrite_rule);
  assert_false(parse_config(raw_config, LL_CONTEXT_ROOT, NULL, NULL), ASSERTION_ERROR("Parsing the given configuration failed"));
  cfg_free(configuration);
};

LogRewrite *
create_rewrite_rule(const char *raw_rewrite_rule)
{
  char raw_config[1024];

  configuration = cfg_new(0x306);
  snprintf(raw_config, sizeof(raw_config), "rewrite s_test{ %s }; log{ rewrite(s_test); };", raw_rewrite_rule);
  assert_true(parse_config(raw_config, LL_CONTEXT_ROOT, NULL, NULL), ASSERTION_ERROR("Parsing the given configuration failed"));
  assert_true(cfg_init(configuration), ASSERTION_ERROR("Config initialization failed"));

  LogExprNode *expr_node = cfg_tree_get_object(&configuration->tree, ENC_REWRITE, "s_test");
  LogPipe* pipe = (LogPipe*)expr_node->children->object;
  return pipe;
}

LogMessage *
create_message_with_fields(const char *field_name, ...)
{
  va_list args;
  const char* arg;
  LogMessage *msg = log_msg_new_empty();
  arg = field_name;
  va_start(args, field_name);
  while (arg != NULL)
    {
      NVHandle handle = log_msg_get_value_handle(arg);
      arg = va_arg(args, char*);
      log_msg_set_value(msg, handle, arg, -1);
      arg = va_arg(args, char*);
    }
  va_end(args);
  return msg;
}

LogMessage *
create_message_with_field(const char *field_name, const char *field_value)
{
  return create_message_with_fields(field_name, field_value, NULL);
}

void
log_pipe_queue_with_default_path_options(LogRewrite *pipe, LogMessage *msg)
{
  LogPathOptions po = LOG_PATH_OPTIONS_INIT;
  log_pipe_queue((LogPipe *) pipe, log_msg_ref(msg), &po);
};

void
rewrite_teardown(LogMessage* msg)
{
  log_msg_unref(msg);
  cfg_free(configuration);
}

void
test_condition_success()
{
  LogRewrite *test_rewrite = create_rewrite_rule("set(\"00100\", value(\"device_id\") condition(program(\"ARCGIS\")));");
  LogMessage *msg = create_message_with_field("PROGRAM", "ARCGIS");
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "device_id", "00100", -1, ASSERTION_ERROR("Bad device_id"));
  rewrite_teardown(msg);
}

void
test_reference_on_condition_cloned()
{
  LogRewrite *test_rewrite = create_rewrite_rule("set(\"00100\", value(\"device_id\") condition(program(\"ARCGIS\")));");
  LogPipe *cloned_rule = log_pipe_clone(&test_rewrite->super);
  assert_guint32(test_rewrite->condition->ref_cnt, 2, ASSERTION_ERROR("Bad reference number of condition"));
  log_pipe_unref(cloned_rule);
  assert_guint32(test_rewrite->condition->ref_cnt, 1, ASSERTION_ERROR("Bad reference number of condition"));
  cfg_free(configuration);
}

void test_set_field_exist_and_set_literal_string()
{
  LogRewrite *test_rewrite = create_rewrite_rule("set(\"value\" value(\"field1\") );");
  LogMessage *msg = create_message_with_field("field1", "oldvalue");
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "value", -1, ASSERTION_ERROR("Couldn't set message field"));
  rewrite_teardown(msg);
}

void test_set_field_not_exist_and_set_literal_string()
{
  LogRewrite *test_rewrite = create_rewrite_rule("set(\"value\" value(\"field1\") );");
  LogMessage *msg = log_msg_new_empty();
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "value", -1, ASSERTION_ERROR("Couldn't set message field when it doesn't exists"));
  rewrite_teardown(msg);
}

void test_set_field_exist_and_set_template_string()
{
  LogRewrite *test_rewrite = create_rewrite_rule("set(\"$field2\" value(\"field1\") );");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue", "field2","newvalue", NULL);
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "newvalue", -1, ASSERTION_ERROR("Couldn't set message field with template value"));
  rewrite_teardown(msg);
}

void test_subst_field_exist_and_substring_substituted()
{
  LogRewrite *test_rewrite = create_rewrite_rule("subst(\"substring\" \"substitute\" value(\"field1\") );");
  LogMessage *msg = create_message_with_fields("field1", "asubstringb", NULL);
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "asubstituteb", -1, ASSERTION_ERROR("Couldn't subst message field with literal"));
  rewrite_teardown(msg);
}

void test_subst_field_exist_and_substring_substituted_with_template()
{
  LogRewrite *test_rewrite = create_rewrite_rule("subst(\"substring\" \"$field2\" value(\"field1\") );");
  LogMessage *msg = create_message_with_fields("field1", "asubstringb", "field2","substitute", NULL);
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "asubstituteb", -1, ASSERTION_ERROR("Couldn't subst message field with template"));
  rewrite_teardown(msg);
}

void test_subst_field_exist_and_substring_substituted_only_once_without_global()
{
  LogRewrite *test_rewrite = create_rewrite_rule("subst(\"substring\" \"substitute\" value(\"field1\") );");
  LogMessage *msg = create_message_with_fields("field1", "substring substring", NULL);
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "substitute substring", -1, ASSERTION_ERROR("Field substituted more than once without global flag"));
  rewrite_teardown(msg);
}

void test_subst_field_exist_and_substring_substituted_every_occurence_with_global()
{
  LogRewrite *test_rewrite = create_rewrite_rule("subst(\"substring\" \"substitute\" value(\"field1\") flags(global) );");
  LogMessage *msg = create_message_with_fields("field1", "substring substring", NULL);
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "substitute substitute", -1, ASSERTION_ERROR("Field substituted less than every occurence with global flag"));
  rewrite_teardown(msg);
}

void test_subst_field_exist_and_substring_substituted_when_regexp_matched()
{
  LogRewrite *test_rewrite = create_rewrite_rule("subst(\"[0-9]+\" \"substitute\" value(\"field1\") );");
  LogMessage *msg = create_message_with_fields("field1", "a123b", NULL);
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "asubstituteb", -1, ASSERTION_ERROR("Couldn't subst message field with literal"));
  rewrite_teardown(msg);
}

void test_set_field_exist_and_group_set_literal_string()
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"value\" values(\"field1\") );");
  LogMessage *msg = create_message_with_field("field1", "oldvalue");
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "value", -1, ASSERTION_ERROR("Couldn't set message field"));
  rewrite_teardown(msg);
}

void test_set_field_exist_and_group_set_multiple_fields_with_glob_pattern_literal_string()
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"value\" values(\"field.*\") );");
  LogMessage *msg = create_message_with_fields("field.name1", "oldvalue","field.name2", "oldvalue", NULL);
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field.name1", "value", -1, ASSERTION_ERROR("Couldn't set message field"));
  assert_msg_field_equals(msg, "field.name2", "value", -1, ASSERTION_ERROR("Couldn't set message field"));
  rewrite_teardown(msg);
}

void test_set_field_exist_and_group_set_multiple_fields_with_glob_question_mark_pattern_literal_string()
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"value\" values(\"field?\") );");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue","field2", "oldvalue", NULL);
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "value", -1, ASSERTION_ERROR("Couldn't set message field"));
  assert_msg_field_equals(msg, "field2", "value", -1, ASSERTION_ERROR("Couldn't set message field"));
  rewrite_teardown(msg);
}

void test_set_field_exist_and_group_set_multiple_fields_with_multiple_glob_pattern_literal_string()
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"value\" values(\"field1\" \"field2\") );");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue","field2", "oldvalue", NULL);
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "value", -1, ASSERTION_ERROR("Couldn't set message field"));
  assert_msg_field_equals(msg, "field2", "value", -1, ASSERTION_ERROR("Couldn't set message field"));
  rewrite_teardown(msg);
}

void test_set_field_exist_and_group_set_template_string()
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"$field2\" values(\"field1\") );");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue", "field2", "value", NULL);
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "value", -1, ASSERTION_ERROR("Couldn't set message field"));
  rewrite_teardown(msg);
}

void test_set_field_exist_and_group_set_template_string_with_old_value()
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"$_ alma\" values(\"field1\") );");
  LogMessage *msg = create_message_with_field("field1", "value");
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "value alma", -1, ASSERTION_ERROR("Couldn't set message field"));
  rewrite_teardown(msg);
}

void test_set_field_exist_and_group_set_when_condition_doesnt_match()
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"value\" values(\"field1\") condition( program(\"program1\") ) );");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue", "PROGRAM", "program2", NULL);
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "oldvalue", -1, ASSERTION_ERROR("Shouldn't rewrite when condition doesn't match"));
  rewrite_teardown(msg);
}

void test_set_field_exist_and_group_set_when_condition_matches()
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"value\" values(\"field1\") condition( program(\"program\") ) );");
  LogMessage *msg = create_message_with_fields("field1", "oldvalue", "PROGRAM", "program", NULL);
  log_pipe_queue_with_default_path_options(test_rewrite, msg);
  assert_msg_field_equals(msg, "field1", "value", -1, ASSERTION_ERROR("Shouldn't rewrite when condition doesn't match"));
  rewrite_teardown(msg);
}

void test_set_field_cloned()
{
  LogRewrite *test_rewrite = create_rewrite_rule("groupset(\"value\" values(\"field1\") condition( program(\"program\") ) );");
  LogPipe *cloned_rule = log_pipe_clone(&test_rewrite->super);
  assert_true(cloned_rule != NULL, ASSERTION_ERROR("Can't cloned the rewrite"));
  log_pipe_unref(cloned_rule);
  cfg_free(configuration);
}

void test_set_field_invalid_template()
{
  expect_config_parse_failure("groupset(\"${alma\" values(\"field1\") );");
}

int
main(int argc, char **argv)
{
  app_startup();
  putenv("TZ=MET-1METDST");
  tzset();

  start_grabbing_messages();
  test_condition_success();
  test_reference_on_condition_cloned();
  test_set_field_exist_and_set_literal_string();
  test_set_field_not_exist_and_set_literal_string();
  test_set_field_exist_and_set_template_string();
  test_subst_field_exist_and_substring_substituted();
  test_subst_field_exist_and_substring_substituted_with_template();
  test_subst_field_exist_and_substring_substituted_only_once_without_global();
  test_subst_field_exist_and_substring_substituted_every_occurence_with_global();
  test_subst_field_exist_and_substring_substituted_when_regexp_matched();
  test_set_field_exist_and_group_set_literal_string();
  test_set_field_exist_and_group_set_multiple_fields_with_glob_pattern_literal_string();
  test_set_field_exist_and_group_set_multiple_fields_with_glob_question_mark_pattern_literal_string();
  test_set_field_exist_and_group_set_multiple_fields_with_multiple_glob_pattern_literal_string();
  test_set_field_exist_and_group_set_template_string();
  test_set_field_exist_and_group_set_template_string_with_old_value();
  test_set_field_invalid_template();
  test_set_field_exist_and_group_set_when_condition_doesnt_match();
  test_set_field_exist_and_group_set_when_condition_matches();
  test_set_field_cloned();
  stop_grabbing_messages();
}
