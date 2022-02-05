/*
 * Copyright (c) 2022 Balázs Scheidler <bazsi77@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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
#include "libtest/cr_template.h"

#include "logscheduler.h"
#include "apphook.h"

typedef struct TestPipe
{
  LogPipe super;
  GMutex lock;
  GQueue *messages;
  gsize messages_count;
} TestPipe;

static void
test_pipe_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  TestPipe *self = (TestPipe *) s;

  g_mutex_lock(&self->lock);
  g_queue_push_tail(self->messages, msg);
  self->messages_count++;
  g_mutex_unlock(&self->lock);
}

static void
test_pipe_free(LogPipe *s)
{
  TestPipe *self = (TestPipe *) s;

  g_queue_free_full(self->messages, (GDestroyNotify) log_msg_unref);
  g_mutex_clear(&self->lock);
  log_pipe_free_method(s);
}

TestPipe *
test_pipe_new(GlobalConfig *cfg)
{
  TestPipe *self = g_new0(TestPipe, 1);

  log_pipe_init_instance(&self->super, cfg);
  self->super.queue = test_pipe_queue;
  self->super.free_fn = test_pipe_free;
  self->messages = g_queue_new();
  g_mutex_init(&self->lock);
  return self;
}

TestPipe *
_construct_test_pipe(void)
{
  TestPipe *pipe = test_pipe_new(configuration);

  cr_assert(log_pipe_init(&pipe->super));
  return pipe;
}

void
_destroy_test_pipe(TestPipe *pipe)
{
  log_pipe_deinit(&pipe->super);
  log_pipe_unref(&pipe->super);
}

Test(logscheduler, test_log_scheduler_can_be_constructed)
{
  LogSchedulerOptions options;
  TestPipe *test_pipe = _construct_test_pipe();
  LogScheduler *s;

  log_scheduler_options_defaults(&options);
  log_scheduler_options_init(&options, configuration);
  s = log_scheduler_new(&options, &test_pipe->super);

  LogMessage *msg = create_sample_message();
  LogPathOptions path_options = LOG_PATH_OPTIONS_INIT;
  log_scheduler_push(s, msg, &path_options);

  msg = create_sample_message();
  log_scheduler_push(s, msg, &path_options);

  cr_assert(test_pipe->messages_count == 2);
  log_scheduler_free(s);
  _destroy_test_pipe(test_pipe);
}

static void
setup(void)
{
  app_startup();
  configuration = cfg_new_snippet();
  cr_assert(cfg_init(configuration));
}

static void
teardown(void)
{
  cfg_free(configuration);
}

TestSuite(logscheduler, .init = setup, .fini = teardown);
