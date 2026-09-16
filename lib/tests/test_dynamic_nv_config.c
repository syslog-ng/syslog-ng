/*
 * Copyright (c) 2026 One Identity
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include <criterion/criterion.h>

#include "apphook.h"
#include "cfg.h"
#include "libtest/grab-logging.h"
#include "logmsg/logmsg.h"
#include "logmsg/nvtable.h"
#include "msg-format.h"

static GlobalConfig *cfg;

static void
setup(void)
{
  app_startup();
  cfg = cfg_new_snippet();
  start_grabbing_messages();
}

static void
teardown(void)
{
  stop_grabbing_messages();
  reset_grabbed_messages();
  cfg_free(cfg);
  app_shutdown();
}

TestSuite(dynamic_nv_config, .init = setup, .fini = teardown);

Test(dynamic_nv_config, accepts_valid_limits)
{
  cr_assert(cfg_set_dynamic_nv_name_limit(cfg, 0));
  cr_assert_eq(cfg->dynamic_nv_name_limit, 0);
  cr_assert(cfg_set_dynamic_nv_name_limit(cfg, NVHANDLE_MAX_VALUE));
  cr_assert_eq(cfg->dynamic_nv_name_limit, NVHANDLE_MAX_VALUE);
}

Test(dynamic_nv_config, rejects_limits_outside_handle_namespace)
{
  cr_assert_not(cfg_set_dynamic_nv_name_limit(cfg, (guint64) NVHANDLE_MAX_VALUE + 1));
  cr_assert_eq(cfg->dynamic_nv_name_limit, DYNAMIC_NV_NAME_LIMIT_DEFAULT);
}

Test(dynamic_nv_config, validates_overflow_policy)
{
  cr_assert(cfg_set_dynamic_nv_name_overflow(cfg, "drop"));
  cr_assert_eq(cfg->dynamic_nv_name_overflow, DYNAMIC_NV_NAME_OVERFLOW_DROP);
  cr_assert(cfg_set_dynamic_nv_name_overflow(cfg, "reject-message"));
  cr_assert_eq(cfg->dynamic_nv_name_overflow, DYNAMIC_NV_NAME_OVERFLOW_REJECT_MESSAGE);
  cr_assert_not(cfg_set_dynamic_nv_name_overflow(cfg, "invalid"));
}

Test(dynamic_nv_config, cfg_init_activates_dynamic_name_limit)
{
  cr_assert(cfg_set_dynamic_nv_name_limit(cfg, 0));
  cr_assert(cfg_init(cfg));

  cr_assert_eq(log_msg_get_dynamic_value_handle("cfg-init-limit-probe"), LM_V_NONE);

  log_msg_registry_set_dynamic_name_limit(DYNAMIC_NV_NAME_LIMIT_DEFAULT);
  cfg_deinit(cfg);
}

Test(dynamic_nv_config, config_reload_applies_dynamic_name_limit_from_new_config)
{
  GlobalConfig *new_cfg;
  NVHandle existing_handle;

  /* Old config allows registering dynamic names. */
  cr_assert(cfg_init(cfg));
  existing_handle = log_msg_get_dynamic_value_handle("reload-existing-dynamic-name");
  cr_assert_neq(existing_handle, LM_V_NONE);
  cr_assert(cfg_deinit(cfg));

  /* Reloaded config rejects new dynamic names. */
  new_cfg = cfg_new_snippet();
  cr_assert(cfg_set_dynamic_nv_name_limit(new_cfg, 0));
  cr_assert(cfg_init(new_cfg));

  /* Existing names remain valid, but new names obey the reloaded limit. */
  cr_assert_eq(log_msg_get_dynamic_value_handle("reload-new-dynamic-name"), LM_V_NONE);
  cr_assert_eq(log_msg_get_dynamic_value_handle("reload-existing-dynamic-name"), existing_handle);

  cr_assert(cfg_deinit(new_cfg));
  cfg_free(new_cfg);
}

Test(dynamic_nv_config, config_reload_applies_dynamic_overflow_policy_to_msg_format_options)
{
  GlobalConfig *new_cfg;
  MsgFormatOptions old_options = {0};
  MsgFormatOptions new_options = {0};

  msg_format_options_defaults(&old_options);
  msg_format_options_defaults(&new_options);

  /* Old config keeps the default drop behavior. */
  cr_assert(cfg_set_dynamic_nv_name_overflow(cfg, "drop"));
  msg_format_options_init(&old_options, cfg);
  cr_assert_not(old_options.reject_dynamic_nv_names);
  msg_format_options_destroy(&old_options);

  /* Reloaded config switches SDATA parsing to reject messages. */
  new_cfg = cfg_new_snippet();
  cr_assert(cfg_set_dynamic_nv_name_overflow(new_cfg, "reject-message"));
  msg_format_options_init(&new_options, new_cfg);
  cr_assert(new_options.reject_dynamic_nv_names);

  msg_format_options_destroy(&new_options);
  cfg_free(new_cfg);
}

Test(dynamic_nv_config, warns_for_large_memory_budget)
{
  cr_assert(cfg_set_dynamic_nv_name_limit(cfg, DYNAMIC_NV_NAME_LIMIT_WARNING_THRESHOLD));
  assert_grabbed_log_contains("permanent memory growth");
}
