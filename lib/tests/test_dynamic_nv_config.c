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
#include "logmsg/nvtable.h"

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

Test(dynamic_nv_config, warns_for_large_memory_budget)
{
  cr_assert(cfg_set_dynamic_nv_name_limit(cfg, DYNAMIC_NV_NAME_LIMIT_WARNING_THRESHOLD));
  assert_grabbed_log_contains("permanent memory growth");
}
