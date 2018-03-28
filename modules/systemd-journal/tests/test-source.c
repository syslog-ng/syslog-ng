/*
 * Copyright (c) 2010-2014 Balabit
 * Copyright (c) 2010-2014 Viktor Juhasz <viktor.juhasz@balabit.com>
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
#include "test-source.h"
#include "syslog-ng.h"
#include "testutils.h"
#include "stats/stats.h"

struct _TestSource
{
  LogPipe super;
  JournalReaderOptions options;
  JournalReader *reader;
  Journald *journald_mock;
  GList *tests;
  GList *current_test;
  TestCase *current_test_case;

  struct iv_task start;
  struct iv_task stop;
  struct iv_task work;
};

static gboolean
__init(LogPipe *s)
{
  TestSource *self = (TestSource *)s;
  self->reader = journal_reader_new(configuration, self->journald_mock);
  journal_reader_options_defaults(&self->options);
  if (self->current_test_case && self->current_test_case->init)
    {
      self->current_test_case->init(self->current_test_case, self, self->journald_mock, self->reader, &self->options);
    }
  journal_reader_options_init(&self->options, configuration, "test");
  journal_reader_set_options((LogPipe *)self->reader, &self->super, &self->options, "test", "1");
  log_pipe_append((LogPipe *)self->reader, &self->super);
  assert_true(log_pipe_init((LogPipe *)self->reader), ASSERTION_ERROR("Can't initialize reader"));
  return TRUE;
}

static gboolean
__deinit(LogPipe *s)
{
  TestSource *self = (TestSource *)s;
  journal_reader_options_destroy(&self->options);
  log_pipe_deinit((LogPipe *)self->reader);
  log_pipe_unref((LogPipe *)self->reader);
  return TRUE;
}

static void
__free(LogPipe *s)
{
  TestSource *self = (TestSource *)s;
  journald_free(self->journald_mock);
  g_list_free(self->tests);
}

static void
__queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  TestSource *self = (TestSource *)s;
  if (self->current_test_case && self->current_test_case->checker)
    {
      self->current_test_case->checker(self->current_test_case, self, msg);
    }
  else
    {
      iv_quit();
    }
  log_msg_drop(msg, path_options, AT_PROCESSED);
}

static void
__start_source(gpointer user_data)
{
  TestSource *self = (TestSource *)user_data;
  log_pipe_init(&self->super);
}

static void
__stop_source(gpointer user_data)
{
  TestSource *self = (TestSource *)user_data;
  log_pipe_deinit(&self->super);
  iv_quit();
}

TestSource *
test_source_new(GlobalConfig *cfg)
{
  TestSource *self = g_new0(TestSource, 1);
  log_pipe_init_instance(&self->super, cfg);
  self->super.init = __init;
  self->super.deinit = __deinit;
  self->super.free_fn = __free;
  self->super.queue = __queue;
  self->journald_mock = journald_mock_new();
  journal_reader_options_defaults(&self->options);

  IV_TASK_INIT(&self->start);
  self->start.cookie = self;
  self->start.handler = __start_source;
  IV_TASK_INIT(&self->stop);
  self->stop.cookie = self;
  self->stop.handler = __stop_source;
  return self;
}

void
test_source_add_test_case(TestSource *self, TestCase *tc)
{
  self->tests = g_list_append(self->tests, tc);
}

void
test_source_run_tests(TestSource *self)
{
  self->current_test = self->tests;
  while (self->current_test)
    {
      self->current_test_case = self->current_test->data;
      iv_task_register(&self->start);
      iv_main();
      log_pipe_deinit(&self->super);
      self->current_test = self->current_test->next;
    }
}

void
test_source_finish_tc(TestSource *self)
{
  if (self->current_test_case && self->current_test_case->finish)
    {
      self->current_test_case->finish(self->current_test_case);
    }
  iv_task_register(&self->stop);
}


