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
#include <logmsg/logmsg.h>
#include <apphook.h>
#include "logthrdest/logthrdestdrv.h"
#include "kafka-dest-driver.h"
#include "kafka-dest-driver.c"
#include "kafka-dest-worker.h"
#include "kafka-dest-worker.c"
#include "debugger/debugger.h"
#include <librdkafka/rdkafka.h>

#include <criterion/criterion.h>
#include <criterion/parameterized.h>

MainLoop *main_loop;
LogDriver *driver;
KafkaDestWorker *worker;
MainLoopOptions main_loop_options;
LogTemplate *topic_name;
KafkaDestDriver *kafka_driver;

static void
_init_topic_names(void)
{
  topic_name = log_template_new(configuration, NULL);
  log_template_compile(topic_name, "topicname", NULL);
  kafka_dd_set_topic(driver, topic_name);
  kafka_dd_set_fallback_topic(driver, "fallbacktopicname");
}

static void
_init_valid_template_topic_names(void)
{
  topic_name = log_template_new(configuration, NULL);
  log_template_compile(topic_name, "$MSG", NULL);
  kafka_dd_set_topic(driver, topic_name);
  kafka_dd_set_fallback_topic(driver, "fallbacktopicname");
}

static void
_init_topics(void)
{
  kafka_driver->kafka = _construct_client(kafka_driver);
  kafka_driver->topic = _construct_topic(kafka_driver, kafka_driver->topic_name->template);
  kafka_driver->fallback_topic = _construct_topic(kafka_driver, kafka_driver->fallback_topic_name);
}

static void
setup(void)
{
  debug_flag = TRUE;
  log_stderr = TRUE;
  app_startup();

  main_loop = main_loop_get_instance();
  main_loop_init(main_loop, &main_loop_options);

  driver = kafka_dd_new(main_loop_get_current_config(main_loop));
  _init_topic_names();
  kafka_driver = (KafkaDestDriver *) driver;
}

static void
teardown(void)
{
  main_loop_sync_worker_startup_and_teardown();
  log_pipe_deinit((LogPipe *)driver);
  log_pipe_unref((LogPipe *)driver);

  main_loop_deinit(main_loop);
  log_template_unref(topic_name);
  app_shutdown();
}

TestSuite(kafka_topic, .init = setup, .fini = teardown);

Test(kafka_topic, test_topic_names)
{
  cr_assert_str_eq(kafka_driver->topic_name->template, "topicname");
  cr_assert_str_eq(kafka_driver->fallback_topic_name, "fallbacktopicname");
}

Test(kafka_topic, test_topics)
{
  _init_topics();
  cr_assert_str_eq(rd_kafka_topic_name(kafka_driver->topic), kafka_driver->topic_name->template);
  cr_assert_str_eq(rd_kafka_topic_name(kafka_driver->fallback_topic), kafka_driver->fallback_topic_name);
}

Test(kafka_topic, test_topic_name_validity)
{
  cr_assert(_validate_topic_name(kafka_driver, "test 1") == FALSE);
  cr_assert(_validate_topic_name(kafka_driver, "") == FALSE);
  cr_assert(_validate_topic_name(kafka_driver, "..") == FALSE);
  cr_assert(_validate_topic_name(kafka_driver, ".") == FALSE);
  cr_assert(_validate_topic_name(kafka_driver, " invalidname") == FALSE);
  cr_assert(_validate_topic_name(kafka_driver, "aba#2") == FALSE);
  cr_assert(_validate_topic_name(kafka_driver, g_strnfill(250, 'a')) == FALSE);

  cr_assert(_validate_topic_name(kafka_driver, "validname123") == TRUE);
  cr_assert(_validate_topic_name(kafka_driver, "valid.name123") == TRUE);
  cr_assert(_validate_topic_name(kafka_driver, "valid_name123") == TRUE);
  cr_assert(_validate_topic_name(kafka_driver, "valid-name123") == TRUE);
  cr_assert(_validate_topic_name(kafka_driver, g_strnfill(249, 'a')) == TRUE);
}


Test(kafka_topic, test_valid_non_template_calculate_topic)
{
  _init_topics();
  worker = (KafkaDestWorker *) kafka_dest_worker_new(&kafka_driver->super, 0);
  LogMessage *msg = log_msg_new_empty();
  cr_assert_str_eq(rd_kafka_topic_name(_calculate_topic(worker, msg)), "topicname");
  log_threaded_dest_worker_free(&worker->super);
}

Test(kafka_topic, test_valid_template_calculate_topic)
{
  _init_valid_template_topic_names();
  _init_topics();

  cr_assert_str_eq(kafka_driver->topic_name->template, "$MSG");
  worker = (KafkaDestWorker *) kafka_dest_worker_new(&kafka_driver->super, 0);
  LogMessage *msg = log_msg_new_empty();

  log_msg_set_value_by_name(msg, "MSG", "valid_template_topic", -1);
  cr_assert_str_eq(rd_kafka_topic_name(_calculate_topic(worker, msg)), "valid_template_topic");
  log_msg_set_value_by_name(msg, "MSG", "valid.template.topic", -1);
  cr_assert_str_eq(rd_kafka_topic_name(_calculate_topic(worker, msg)), "valid.template.topic");
  log_msg_set_value_by_name(msg, "MSG", "valid-template-topic", -1);
  cr_assert_str_eq(rd_kafka_topic_name(_calculate_topic(worker, msg)), "valid-template-topic");

  log_threaded_dest_worker_free(&worker->super);
}

Test(kafka_topic, test_invalid_template_calculate_topic)
{
  _init_valid_template_topic_names();
  _init_topics();

  cr_assert_str_eq(kafka_driver->topic_name->template, "$MSG");
  worker = (KafkaDestWorker *) kafka_dest_worker_new(&kafka_driver->super, 0);
  LogMessage *msg = log_msg_new_empty();

  log_msg_set_value_by_name(msg, "MSG", "invalid topic", -1);
  cr_assert_str_eq(rd_kafka_topic_name(_calculate_topic(worker, msg)), "fallbacktopicname");
  log_msg_set_value_by_name(msg, "MSG", "..", -1);
  cr_assert_str_eq(rd_kafka_topic_name(_calculate_topic(worker, msg)), "fallbacktopicname");
  log_msg_set_value_by_name(msg, "MSG", "", -1);
  cr_assert_str_eq(rd_kafka_topic_name(_calculate_topic(worker, msg)), "fallbacktopicname");

  log_threaded_dest_worker_free(&worker->super);
}
