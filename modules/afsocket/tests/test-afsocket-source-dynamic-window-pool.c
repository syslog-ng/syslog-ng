/*
 * Copyright (c) 2026 Akos Zalavary <akos.zalavary@logiqum.com>
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

#include "afinet-source.h"
#include "apphook.h"
#include "cfg.h"
#include "stats/stats-registry.h"

/*
 * A reload hands the dynamic window pool of a source from the old configuration to the new one, or back to the
 * old one if the reload is reverted. The connections kept from the old configuration keep allocating from the pool
 * they were created with, so a reverted reload must bring that very pool back: a different one would let the
 * old and the new connections exceed dynamic-window-size() together. The test holds a reference to the pool, as
 * a kept connection does, and compares pool identities across the reload cycle.
 */

#define DYNAMIC_WINDOW_SIZE 10000

typedef struct
{
  GlobalConfig *cfg;
  AFInetSourceDriver *driver;
} TestSource;

static void
_start_source(TestSource *source, gboolean keep_alive, gsize dynamic_window_size)
{
  source->cfg = cfg_new_snippet();
  source->driver = afinet_sd_new_tcp(source->cfg);

  LogDriver *driver = &source->driver->super.super.super;
  afinet_sd_set_localip(driver, "127.0.0.1");
  afinet_sd_set_localport(driver, "0");
  afsocket_sd_set_keep_alive(driver, keep_alive);
  afsocket_sd_set_dynamic_window_size(driver, dynamic_window_size);
  source->driver->super.socket_options->so_reuseport = TRUE;
}

static void
_free_source(TestSource *source)
{
  if (source->cfg->persist)
    {
      persist_config_free(source->cfg->persist);
      source->cfg->persist = NULL;
    }
  log_pipe_unref(&source->driver->super.super.super.super);
  cfg_free(source->cfg);
}

/* the way main_loop_reload_config_apply() and _revert() hand over the state: deinit the configuration into
 * its persist (a new one for the running configuration, the moved one for the configuration that is replaced
 * again), then move the persist to the configuration that comes next */
static void
_stop_source(TestSource *source, GlobalConfig *next_cfg)
{
  if (!source->cfg->persist)
    source->cfg->persist = persist_config_new();
  cr_assert(log_pipe_deinit(&source->driver->super.super.super.super));
  cfg_persist_config_move(source->cfg, next_cfg);
}

static void
_init_source(TestSource *source)
{
  cr_assert(log_pipe_init(&source->driver->super.super.super.super));
}

static DynamicWindowPool *
_pool_of(TestSource *source)
{
  return source->driver->super.dynamic_window_pool;
}

static void
_assert_reverted_reload_returns_the_pool(gboolean old_keep_alive, gboolean new_keep_alive, gsize new_size)
{
  TestSource old_config, new_config;

  _start_source(&old_config, old_keep_alive, DYNAMIC_WINDOW_SIZE);
  _start_source(&new_config, new_keep_alive, new_size);
  /* the persist names of the two sources must be the same, as they come from the same config statement */
  log_pipe_set_persist_name(&old_config.driver->super.super.super.super, "network_source");
  log_pipe_set_persist_name(&new_config.driver->super.super.super.super, "network_source");

  _init_source(&old_config);
  DynamicWindowPool *original_pool = _pool_of(&old_config);
  cr_assert_not_null(original_pool);
  dynamic_window_pool_ref(original_pool);   /* held by a connection kept from the old configuration */

  /* the reload: the new configuration initializes, and then a later source of it fails */
  _stop_source(&old_config, new_config.cfg);
  _init_source(&new_config);
  _stop_source(&new_config, old_config.cfg);

  /* the revert: the old configuration is initialized again */
  _init_source(&old_config);
  cr_assert_eq(_pool_of(&old_config), original_pool,
               "the reverted source must get back the pool its kept connections allocate from");

  cr_assert(log_pipe_deinit(&old_config.driver->super.super.super.super));
  dynamic_window_pool_unref(original_pool);
  _free_source(&old_config);
  _free_source(&new_config);
}

Test(afsocket_source_dynamic_window_pool, reverted_reload_returns_the_pool_that_the_new_source_does_not_take_over)
{
  _assert_reverted_reload_returns_the_pool(TRUE, FALSE, DYNAMIC_WINDOW_SIZE);
}

Test(afsocket_source_dynamic_window_pool, reverted_reload_returns_the_pool_when_the_new_source_turns_it_off)
{
  _assert_reverted_reload_returns_the_pool(TRUE, FALSE, 0);
}

Test(afsocket_source_dynamic_window_pool, reverted_reload_returns_the_pool_that_the_new_source_takes_over)
{
  _assert_reverted_reload_returns_the_pool(TRUE, TRUE, DYNAMIC_WINDOW_SIZE);
}

static void
setup(void)
{
  app_startup();
}

static void
teardown(void)
{
  app_shutdown();
}

TestSuite(afsocket_source_dynamic_window_pool, .init = setup, .fini = teardown);
