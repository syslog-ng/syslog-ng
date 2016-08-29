#include "add-contextual-data-template-selector.h"
#include "logmsg/logmsg.h"
#include "template/macros.h"
#include "cfg.h"
#include "apphook.h"
#include <criterion/criterion.h>

static LogMessage*
_create_log_msg(const gchar *message, const gchar *host)
{
  LogMessage *msg = NULL;
  msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_MESSAGE, message, -1);
  log_msg_set_value(msg, LM_V_HOST, host, -1);

  return msg;
}

Test(add_contextual_data_template_selector, test_given_empty_selector_when_resolve_then_result_is_null)
{
  AddContextualDataSelector *selector = NULL;
  LogMessage *msg = _create_log_msg("testmsg", "localhost");

  cr_assert_eq(add_contextual_data_selector_resolve(selector, msg), NULL, "When selector is NULL the resolve should return NULL.");
  log_msg_unref(msg);
}

static AddContextualDataSelector*
_create_template_selector(const gchar *template_string)
{
  GlobalConfig *cfg = cfg_new(VERSION_VALUE);
  AddContextualDataSelector *selector = add_contextual_data_template_selector_new(cfg, template_string);
  add_contextual_data_selector_init(selector);

  return selector;
}

Test(add_contextual_data_template_selector, test_given_template_selector_when_resolve_then_result_is_the_formatted_template_value)
{
  AddContextualDataSelector *selector = _create_template_selector("$HOST");
  LogMessage *msg = _create_log_msg("testmsg", "localhost");
  gchar *resolved_selector = add_contextual_data_selector_resolve(selector, msg);

  cr_assert_str_eq(resolved_selector, "localhost", "");
  g_free(resolved_selector);
  log_msg_unref(msg);
  add_contextual_data_selector_free(selector);
}

__attribute__((constructor))
static void global_test_init(void)
{
  app_startup();
}

__attribute__((destructor))
static void global_test_deinit(void)
{
  app_shutdown();
}
