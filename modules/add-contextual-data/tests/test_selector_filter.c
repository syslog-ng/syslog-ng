/*
 * Copyright (c) 2018 Balabit
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
#include "add-contextual-data-filter-selector.h"
#include "logmsg/logmsg.h"
#include "template/macros.h"
#include "license_module_mock.h"
#include "cfg.h"
#include "apphook.h"
#include <criterion/criterion.h>
#include <unistd.h>

static gchar *test_filter_conf;

static LogMessage *
_create_log_msg(const gchar *message, const gchar *host)
{
  LogMessage *msg = NULL;
  msg = log_msg_new_empty();
  log_msg_set_value(msg, LM_V_MESSAGE, message, -1);
  log_msg_set_value(msg, LM_V_HOST, host, -1);

  return msg;
}


static gchar *
_setup_filter_cfg(const gchar *cfg_content, gint size)
{
  gchar tmp_filename[] = "testfiltersXXXXXX";
  gint fd = mkstemp(tmp_filename);
  ssize_t retval = write(fd, cfg_content, size);
  if (retval > 0)
    close(fd);

  return g_strdup(tmp_filename);
}

static AddContextualDataSelector *
_create_filter_selector(const gchar *filter_cfg, gint size, GList *ordered_filters)
{
  GlobalConfig *cfg = cfg_new(VERSION_VALUE);
  test_filter_conf = _setup_filter_cfg(filter_cfg, size);
  AddContextualDataSelector *selector = add_contextual_data_selector_filter_new(cfg, test_filter_conf);
  if (!add_contextual_data_selector_init(selector, ordered_filters))
    return NULL;

  return selector;
}

static void
setup(void)
{
  app_startup();
  /* Force to link the libtest library */
  license_module_init(NULL,NULL);
  test_filter_conf = NULL;
}

static void
teardown(void)
{
  app_shutdown();
  if (test_filter_conf)
    {
      unlink(test_filter_conf);
    }
  g_free(test_filter_conf);
}

TestSuite(add_contextual_data_filter_selector, .init = setup, .fini = teardown);

Test(add_contextual_data_filter_selector, test_clone_selector_with_filters)
{
  const gchar cfg_content[] = "filter f_localhost {"\
                              "   host(\"localhost\");"\
                              "};";
  GList *ordered_filters = NULL;
  ordered_filters = g_list_append(ordered_filters, "f_localhost");
  AddContextualDataSelector *selector = _create_filter_selector(cfg_content, strlen(cfg_content), ordered_filters);
  AddContextualDataSelector *cloned_selector = add_contextual_data_selector_clone(selector, cfg_new(VERSION_VALUE));
  LogMessage *msg = _create_log_msg("testmsg", "localhost");
  gchar *resolved_selector = add_contextual_data_selector_resolve(cloned_selector, msg);

  cr_assert_str_eq(resolved_selector, "f_localhost", "Filter name is resolved.");
  g_free(resolved_selector);
}

Test(add_contextual_data_filter_selector, test_matching_host_filter_selection)
{
  const gchar cfg_content[] = "filter f_localhost {"\
                              "   host(\"localhost\");"\
                              "};";
  GList *ordered_filters = NULL;
  ordered_filters = g_list_append(ordered_filters, "f_localhost");
  AddContextualDataSelector *selector = _create_filter_selector(cfg_content, strlen(cfg_content), ordered_filters);
  LogMessage *msg = _create_log_msg("testmsg", "localhost");
  gchar *resolved_selector = add_contextual_data_selector_resolve(selector, msg);

  cr_assert_str_eq(resolved_selector, "f_localhost", "Filter name is resolved.");
  g_free(resolved_selector);
}


Test(add_contextual_data_filter_selector, test_matching_msg_filter_selection)
{
  const gchar cfg_content[] = "filter f_msg {"\
                              "    message(\"testmsg\");"\
                              "};";
  GList *ordered_filters = NULL;
  ordered_filters = g_list_append(ordered_filters, "f_msg");
  AddContextualDataSelector *selector = _create_filter_selector(cfg_content, strlen(cfg_content), ordered_filters);
  LogMessage *msg = _create_log_msg("testmsg", "localhost");
  gchar *resolved_selector = add_contextual_data_selector_resolve(selector, msg);

  cr_assert_str_eq(resolved_selector, "f_msg", "Filter name is resolved.");
  g_free(resolved_selector);
}

Test(add_contextual_data_filter_selector, test_matching_host_and_msg_filter_selection)
{
  const gchar cfg_content[] = "filter f_localhost {"\
                              "    host(\"localhost\");"\
                              "};"\
                              "filter f_msg {"\
                              "    message(\"testmsg\");"\
                              "};";
  GList *ordered_filters = NULL;
  ordered_filters = g_list_append(ordered_filters, "f_msg");
  ordered_filters = g_list_append(ordered_filters, "f_localhost");
  AddContextualDataSelector *selector = _create_filter_selector(cfg_content, strlen(cfg_content), ordered_filters);
  LogMessage *msg = _create_log_msg("testmsg", "localhost");
  gchar *resolved_selector = add_contextual_data_selector_resolve(selector, msg);

  cr_assert_str_eq(resolved_selector, "f_msg", "Message filter name is resolved");
  g_free(resolved_selector);
}

Test(add_contextual_data_filter_selector, test_invalid_filter_config)
{
  const gchar cfg_content[] = "filter f_localhost {"\
                              "   bad-filter-name(\"localhost\");"\
                              "};";
  AddContextualDataSelector *selector = _create_filter_selector(cfg_content, strlen(cfg_content), NULL);
  cr_assert_null(selector, "Filter selector cannot be initialized.");
}

