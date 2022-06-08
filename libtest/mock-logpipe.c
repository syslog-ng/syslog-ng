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
#include "mock-logpipe.h"

LogMessage *
log_pipe_mock_get_message(LogPipeMock *self, gint ndx)
{
  g_assert(ndx >= 0 && ndx < self->captured_messages->len);
  return (LogMessage *) g_ptr_array_index(self->captured_messages, ndx);
}

static void
log_pipe_mock_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  LogPipeMock *self = (LogPipeMock *) s;

  g_ptr_array_add(self->captured_messages, log_msg_ref(msg));
  log_pipe_forward_msg(s, msg, path_options);
}

static void
log_pipe_mock_free(LogPipe *s)
{
  LogPipeMock *self = (LogPipeMock *) s;

  g_ptr_array_free(self->captured_messages, TRUE);
  log_pipe_free_method(s);
}

LogPipeMock *
log_pipe_mock_new(GlobalConfig *cfg)
{
  LogPipeMock *self = g_new0(LogPipeMock, 1);

  log_pipe_init_instance(&self->super, cfg);
  self->captured_messages = g_ptr_array_new_full(0, (GDestroyNotify) log_msg_unref);
  self->super.queue = log_pipe_mock_queue;
  self->super.free_fn = log_pipe_mock_free;
  return self;
}
