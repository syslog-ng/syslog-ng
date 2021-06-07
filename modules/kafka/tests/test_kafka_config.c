/*
 * Copyright (c) 2021 One Identity
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
#include "apphook.h"
#include "grab-logging.h"
#include "kafka-dest-driver.h"
#include "kafka-dest-worker.h"
#include "kafka-internal.h"

#include <criterion/criterion.h>

static void
_setup_topic(LogDriver *driver, const gchar *topic_name)
{
  GlobalConfig *cfg = log_pipe_get_config(&driver->super);
  LogTemplate *topic_template = log_template_new(cfg, NULL);
  log_template_compile(topic_template, topic_name, NULL);
  kafka_dd_set_topic(driver, topic_template);
}

Test(kafka_config, test_topic_is_mandatory)
{
  LogDriver *driver = kafka_dd_new(configuration);
  kafka_dd_set_bootstrap_servers(driver, "localhost:9092");
  cr_assert_not(kafka_dd_init(&driver->super));
  assert_grabbed_log_contains("kafka: the topic() argument is required for kafka destinations");

  log_pipe_deinit(&driver->super);
  log_pipe_unref(&driver->super);
}

Test(kafka_config, test_bootstrap_server_is_mandatory)
{
  LogDriver *driver = kafka_dd_new(configuration);
  _setup_topic(driver, "test-topic");
  cr_assert_not(kafka_dd_init(&driver->super));
  assert_grabbed_log_contains("kafka: the bootstrap-servers() option is required for kafka destinations");

  log_pipe_deinit(&driver->super);
  log_pipe_unref(&driver->super);
}

Test(kafka_config, test_mandatory_options)
{
  LogDriver *driver = kafka_dd_new(configuration);
  _setup_topic(driver, "default-test-topic");
  kafka_dd_set_bootstrap_servers(driver, "test-host:9092");
  cr_assert(kafka_dd_init(&driver->super));

  log_pipe_deinit(&driver->super);
  log_pipe_unref(&driver->super);
}

static void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
  start_grabbing_messages();
}

static void
teardown(void)
{
  stop_grabbing_messages();
  app_shutdown();
  cfg_free(configuration);
}

TestSuite(kafka_config, .init = setup, .fini = teardown);
