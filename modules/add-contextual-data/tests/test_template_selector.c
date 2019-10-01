/*
 * Copyright (c) 2016 Balabit
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

#include "add-contextual-data-template-selector.h"
#include "logmsg/logmsg.h"
#include "template/macros.h"
#include "cfg.h"
#include "apphook.h"
#include <criterion/criterion.h>
#include <unistd.h>

TestSuite(add_contextual_data_template_selector, .init = app_startup, .fini = app_shutdown);

static LogMessage *
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

  cr_assert_null(add_contextual_data_selector_resolve(selector, msg),
                 "When selector is NULL the resolve should return NULL.");
  log_msg_unref(msg);
}

static AddContextualDataSelector *
_create_template_selector(const gchar *template_string)
{
  GlobalConfig *cfg = cfg_new_snippet();
  LogTemplate *selector_template = log_template_new(cfg, NULL);

  cr_assert(log_template_compile(selector_template, template_string, NULL));
  AddContextualDataSelector *selector = add_contextual_data_template_selector_new(selector_template);
  add_contextual_data_selector_init(selector, NULL);

  return selector;
}

Test(add_contextual_data_template_selector,
     test_given_template_selector_when_resolve_then_result_is_the_formatted_template_value)
{
  AddContextualDataSelector *selector = _create_template_selector("$HOST");
  LogMessage *msg = _create_log_msg("testmsg", "localhost");
  gchar *resolved_selector = add_contextual_data_selector_resolve(selector, msg);

  cr_assert_str_eq(resolved_selector, "localhost", "");
  log_msg_unref(msg);
  add_contextual_data_selector_free(selector);
}

Test(add_contextual_data_template_selector, test_template_selector_cannot_be_resolved)
{
  AddContextualDataSelector *selector = _create_template_selector("$PROGRAM");
  LogMessage *msg = _create_log_msg("testmsg", "localhost");
  gchar *resolved_selector = add_contextual_data_selector_resolve(selector, msg);

  cr_assert_str_eq(resolved_selector, "", "No template should be resolved.");
  log_msg_unref(msg);
  add_contextual_data_selector_free(selector);
}
