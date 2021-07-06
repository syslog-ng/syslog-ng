/*
 * Copyright (c) 2020 Balabit
 * Copyright (c) 2020 Balazs Scheidler
 * Copyright (c) 2020 Vivin Peris
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
#include <syslog-ng.h>
#include <apphook.h>
#include "logthrdest/logthrdestdrv.h"
#include "kafka-dest-driver.h"
#include <librdkafka/rdkafka.h>
#include "kafka-dest-worker.h"
#include "kafka-dest-driver.h"
#include "kafka-internal.h"

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

#define STRING_250_LEN "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" \
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" \
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" \
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" \
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

#define STRING_249_LEN "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" \
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" \
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" \
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" \
                       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

struct valid_topic_test_params
{
  gchar *topic_name;
};

struct invalid_topic_test_params
{
  gchar *topic_name;
  enum KafkaTopicError type;
};

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

static void
_init_topic_names(LogDriver *driver, const gchar *topic, const gchar *fallback_topic)
{
  LogTemplate *topic_name = log_template_new(configuration, NULL);

  cr_assert(log_template_compile(topic_name, topic, NULL));
  kafka_dd_set_topic(driver, topic_name);
  kafka_dd_set_fallback_topic(driver, fallback_topic);
}

TestSuite(kafka_topic, .init = setup, .fini = teardown);

ParameterizedTestParameters(kafka_topic, valid_topic_tests)
{
  static struct valid_topic_test_params params[] =
  {
    {"validname123"},
    {"valid.name123"},
    {"valid-name123"},
    {"valid_name123"},
    {"valid.name-123"},
    {"valid_name-123"},
    {"This.is.a.valid.name"},
    {"This.is-a_valid-name"},
    {STRING_249_LEN},
  };

  return cr_make_param_array(struct valid_topic_test_params, params, G_N_ELEMENTS(params));
}

ParameterizedTest(struct valid_topic_test_params *param, kafka_topic, valid_topic_tests)
{
  GError *error = NULL;
  cr_assert_eq(kafka_dd_validate_topic_name(param->topic_name, &error), TRUE);
  cr_assert_null(error);
}

ParameterizedTestParameters(kafka_topic, invalid_topic_tests)
{
  static struct invalid_topic_test_params params[] =
  {
    {"test 1", TOPIC_INVALID_PATTERN},
    {"", TOPIC_LENGTH_ZERO},
    {"..", TOPIC_DOT_TWO_DOTS},
    {".", TOPIC_DOT_TWO_DOTS},
    {" invalidname", TOPIC_INVALID_PATTERN},
    {"a*123", TOPIC_INVALID_PATTERN},
    {"topic@123", TOPIC_INVALID_PATTERN},
    {"aba#2", TOPIC_INVALID_PATTERN},
    {"dontuse&", TOPIC_INVALID_PATTERN},
    {"illegal%", TOPIC_INVALID_PATTERN},
    {"name!", TOPIC_INVALID_PATTERN},
    {"topics^", TOPIC_INVALID_PATTERN},
    {"round()", TOPIC_INVALID_PATTERN},
    {"sum+", TOPIC_INVALID_PATTERN},
    {"topic=name", TOPIC_INVALID_PATTERN},
    {"topic``~invalid", TOPIC_INVALID_PATTERN},
    {STRING_250_LEN, TOPIC_EXCEEDS_MAX_LENGTH},
  };

  return cr_make_param_array(struct invalid_topic_test_params, params, G_N_ELEMENTS(params));
}

ParameterizedTest(struct invalid_topic_test_params *param, kafka_topic, invalid_topic_tests)
{
  GError *error = NULL;
  cr_assert_eq(kafka_dd_validate_topic_name(param->topic_name, &error), FALSE);
  cr_assert_eq(error->domain, TOPIC_NAME_ERROR);
  cr_assert_eq(error->code, param->type);
  cr_assert_not_null(error);
  g_error_free(error);
}

Test(kafka_topic, test_resolve_template_topic_name)
{
  configuration = cfg_new_snippet();
  LogDriver *driver = kafka_dd_new(configuration);

  kafka_dd_set_bootstrap_servers(driver, "test-server:9092");
  _init_topic_names(driver, "$kafka_topic", "fallbacktopicname");

  cr_assert(log_pipe_init((LogPipe *) driver));

  KafkaDestDriver *kafka_driver = (KafkaDestDriver *) driver;

  KafkaDestWorker *worker = (KafkaDestWorker *) kafka_dest_worker_new(&kafka_driver->super, 0);

  LogMessage *msg = log_msg_new_empty();
  log_msg_set_value_by_name(msg, "kafka_topic", "valid_template_topic", -1);
  cr_assert_str_eq(kafka_dest_worker_resolve_template_topic_name(worker, msg), "valid_template_topic");

  log_msg_set_value_by_name(msg, "kafka_topic", "invalid name", -1);

  cr_assert_str_eq(kafka_dest_worker_resolve_template_topic_name(worker, msg), "fallbacktopicname");

  log_msg_unref(msg);

  log_threaded_dest_worker_free(&worker->super);
  log_pipe_deinit(&driver->super);
  log_pipe_unref(&driver->super);
  cfg_free(configuration);
}

Test(kafka_topic, test_calculate_topic_from_template)
{
  configuration = cfg_new_snippet();
  LogDriver *driver = kafka_dd_new(configuration);

  kafka_dd_set_bootstrap_servers(driver, "test-server:9092");
  _init_topic_names(driver, "$kafka_topic", "fallbackhere");

  cr_assert(log_pipe_init((LogPipe *) driver));

  KafkaDestDriver *kafka_driver = (KafkaDestDriver *) driver;

  KafkaDestWorker *worker = (KafkaDestWorker *) kafka_dest_worker_new(&kafka_driver->super, 0);

  LogMessage *msg = log_msg_new_empty();

  cr_assert(kafka_dd_is_topic_name_a_template(kafka_driver));

  log_msg_set_value_by_name(msg, "kafka_topic", "validtopic", -1);
  cr_assert_str_eq(rd_kafka_topic_name(kafka_dest_worker_calculate_topic_from_template(worker, msg)), "validtopic");

  log_msg_set_value_by_name(msg, "kafka_topic", "invalid name", -1);

  cr_assert_str_eq(rd_kafka_topic_name(kafka_dest_worker_calculate_topic_from_template(worker, msg)), "fallbackhere");

  log_msg_unref(msg);

  log_threaded_dest_worker_free(&worker->super);
  log_pipe_deinit(&driver->super);
  log_pipe_unref(&driver->super);
  cfg_free(configuration);
}

Test(kafka_topic, test_get_literal_topic)
{
  configuration = cfg_new_snippet();
  LogDriver *driver = kafka_dd_new(configuration);

  kafka_dd_set_bootstrap_servers(driver, "test-server:9092");
  _init_topic_names(driver, "topicname", "fallback");
  cr_assert(log_pipe_init((LogPipe *) driver));

  KafkaDestDriver *kafka_driver = (KafkaDestDriver *) driver;

  KafkaDestWorker *worker = (KafkaDestWorker *) kafka_dest_worker_new(&kafka_driver->super, 0);

  cr_assert_not(kafka_dd_is_topic_name_a_template(kafka_driver));

  LogMessage *msg = log_msg_new_empty();
  cr_assert_str_eq(rd_kafka_topic_name(kafka_dest_worker_get_literal_topic(worker)), "topicname");

  log_msg_unref(msg);
  log_threaded_dest_worker_free(&worker->super);
  log_pipe_deinit(&driver->super);
  log_pipe_unref(&driver->super);
  cfg_free(configuration);
}

Test(kafka_topic, test_calculate_topic)
{
  configuration = cfg_new_snippet();
  LogDriver *driver = kafka_dd_new(configuration);

  kafka_dd_set_bootstrap_servers(driver, "test-server:9092");
  _init_topic_names(driver, "$kafka_topic", "fallbackhere");

  cr_assert(log_pipe_init((LogPipe *) driver));

  KafkaDestDriver *kafka_driver = (KafkaDestDriver *) driver;

  KafkaDestWorker *worker = (KafkaDestWorker *) kafka_dest_worker_new(&kafka_driver->super, 0);

  LogMessage *msg = log_msg_new_empty();

  cr_assert(kafka_dd_is_topic_name_a_template(kafka_driver));

  log_msg_set_value_by_name(msg, "kafka_topic", "validtopic", -1);
  cr_assert_str_eq(rd_kafka_topic_name(kafka_dest_worker_calculate_topic(worker, msg)), "validtopic");

  log_msg_set_value_by_name(msg, "kafka_topic", "invalid name", -1);

  cr_assert_str_eq(rd_kafka_topic_name(kafka_dest_worker_calculate_topic(worker, msg)), "fallbackhere");

  log_msg_unref(msg);

  log_threaded_dest_worker_free(&worker->super);
  log_pipe_deinit(&driver->super);
  log_pipe_unref(&driver->super);
  cfg_free(configuration);
}
