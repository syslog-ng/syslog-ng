/*
 * Copyright (c) 2019 Balazs Scheidler <bazsi77@gmail.com>
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
#include "add-contextual-data-glob-selector.h"
#include "scratch-buffers.h"
#include "string-list.h"
#include "logmsg/logmsg.h"
#include "cfg.h"
#include "apphook.h"

static AddContextualDataSelector *
_create_glob_selector(const gchar *template_string, const gchar *glob1, ...)
{
  GlobalConfig *cfg = cfg_new_snippet();
  LogTemplate *glob_template = log_template_new(cfg, NULL);

  cr_assert(log_template_compile(glob_template, template_string, NULL));
  AddContextualDataSelector *selector = add_contextual_data_glob_selector_new(glob_template);

  va_list va;
  va_start(va, glob1);
  add_contextual_data_selector_init(selector, string_vargs_to_list_va(glob1, va));
  va_end(va);

  AddContextualDataSelector *cloned = add_contextual_data_selector_clone(selector, cfg);
  add_contextual_data_selector_free(selector);
  return cloned;
}

#define _assert_resolved_value(selector, msg, expected_value) \
  do {                    \
    gchar *resolved = add_contextual_data_selector_resolve(selector, msg);  \
    cr_assert_str_eq(resolved, expected_value, "resolved value mismatch: %s != %s", resolved, expected_value);        \
    g_free(resolved);               \
  } while (0)

#define _assert_resolved_value_is_null(selector, msg) \
  do {                    \
    gchar *resolved = add_contextual_data_selector_resolve(selector, msg);  \
    cr_assert_null(resolved, "unexpected non-NULL value: %s", resolved);        \
  } while (0)

Test(add_contextual_data_glob_selector,
     glob_selector_finds_first_expr_that_matches_the_expanded_template)
{
  AddContextualDataSelector *selector = _create_glob_selector("$HOST", "local*", "loc*", "lac*", NULL);

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_HOST, "localhost", -1);
  _assert_resolved_value(selector, msg, "local*");

  log_msg_set_value(msg, LM_V_HOST, "lacsomething", -1);
  _assert_resolved_value(selector, msg, "lac*");

  log_msg_unref(msg);
  add_contextual_data_selector_free(selector);
}

Test(add_contextual_data_glob_selector,
     glob_selector_finds_first_expr_that_matches_the_expanded_template2)
{
  AddContextualDataSelector *selector = _create_glob_selector("$PROGRAM", "unmatch1", "unmatch2", "good*", NULL);

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_PROGRAM, "good", -1);
  _assert_resolved_value(selector, msg, "good*");

  log_msg_set_value(msg, LM_V_HOST, "goodstuff", -1);
  _assert_resolved_value(selector, msg, "good*");

  log_msg_unref(msg);
  add_contextual_data_selector_free(selector);
}

Test(add_contextual_data_glob_selector,
     glob_selector_yields_NULL_if_no_pattern_matches)
{
  AddContextualDataSelector *selector = _create_glob_selector("$PROGRAM", "unmatch1", "unmatch2", "unmatch3", NULL);

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_PROGRAM, "good", -1);
  _assert_resolved_value_is_null(selector, msg);

  log_msg_unref(msg);
  add_contextual_data_selector_free(selector);
}

static void
startup(void)
{
  app_startup();
}

static void
teardown(void)
{
  scratch_buffers_explicit_gc();
  app_shutdown();
}

TestSuite(add_contextual_data_glob_selector, .init = startup, .fini = teardown);
