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
#include <criterion/criterion.h>

#include "test-source.h"
#include "syslog-ng.h"
#include "stats/stats.h"

struct _TestSource
{
  LogPipe super;
  JournalReaderOptions options;
  JournalReader *reader;
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
  GlobalConfig *cfg = log_pipe_get_config(s);

  self->reader = journal_reader_new(cfg);
  journal_reader_options_defaults(&self->options);
  if (self->current_test_case && self->current_test_case->init)
    {
      self->current_test_case->init(self->current_test_case, self, self->reader, &self->options);
    }
  journal_reader_options_init(&self->options, cfg, "test");
  journal_reader_set_options((LogPipe *)self->reader, &self->super, &self->options, "test", NULL);
  log_pipe_append((LogPipe *)self->reader, &self->super);
  cr_assert(log_pipe_init((LogPipe *)self->reader), "%s", "Can't initialize reader");
  return TRUE;
}

static gboolean
__deinit(LogPipe *s)
{
  TestSource *self = (TestSource *)s;
  log_pipe_deinit((LogPipe *)self->reader);
  log_pipe_unref((LogPipe *)self->reader);
  journal_reader_options_destroy(&self->options);
  return TRUE;
}

static void
__free(LogPipe *s)
{
  TestSource *self = (TestSource *)s;
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

void *
journal_reader_test_prepare_with_namespace(const gchar *namespace, GlobalConfig *cfg)
{
  TestSource *test = g_new0(TestSource, 1);

  test->reader = journal_reader_new(cfg);
  log_pipe_init_instance(&test->super, cfg);
  journal_reader_options_defaults(&test->options);
  journal_reader_options_init(&test->options, cfg, "namespace_test");
  journal_reader_set_options((LogPipe *)test->reader, &test->super, &test->options, "namespace_test", NULL);
  log_pipe_append((LogPipe *)test->reader, &test->super);

  if (test->options.namespace) g_free(test->options.namespace);
  test->options.namespace = g_strdup(namespace);

  return test;
}

gboolean
journal_reader_test_allocate_namespace(void *test)
{
  TestSource *self = (TestSource *)test;
  return log_pipe_init((LogPipe *)self->reader);
}

void
journal_reader_test_destroy(void *test)
{
  TestSource *self = (TestSource *)test;
  log_pipe_deinit((LogPipe *)self->reader);
  log_pipe_unref((LogPipe *)self->reader);
  journal_reader_options_destroy(&self->options);
  g_free(test);
  test = NULL;
}
