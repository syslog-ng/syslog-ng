/*
 * Copyright (c) 2002-2007 BalaBit IT Ltd, Budapest, Hungary                    
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * Note that this permission is granted for only version 2 of the GPL.
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
 */
  
#include "logsource.h"

#include "messages.h"


static void
log_source_msg_ack(LogMessage *msg, gpointer user_data)
{
  LogSource *self = (LogSource *) user_data;
  
  g_atomic_counter_inc(&self->options->window_size);
  log_msg_unref(msg);
  
  log_pipe_unref(&self->super);
}

static void
log_source_queue(LogPipe *s, LogMessage *msg, gint path_flags)
{
  LogSource *self = (LogSource *) s;
  
  /* NOTE: we start by turning off PF_FLOW_CTL_OFF, thus we need an acknowledgement */
  path_flags = 0;
  log_msg_ref(msg);
  log_msg_add_ack(msg, path_flags);
  msg->ack_func = log_source_msg_ack;
  msg->ack_userdata = log_pipe_ref(s);
    
  g_atomic_counter_dec_and_test(&self->options->window_size);
  log_pipe_queue(s->pipe_next, msg, path_flags);
}

void
log_source_init_instance(LogSource *self, LogSourceOptions *options)
{
  log_pipe_init_instance(&self->super);
  self->options = options;
  self->super.queue = log_source_queue;
  g_assert(g_atomic_counter_get(&options->window_size) != -1); 
}

void
log_source_options_defaults(LogSourceOptions *options)
{
  options->init_window_size = -1;
  g_atomic_counter_set(&options->window_size, -1);
}

void
log_source_options_init(LogSourceOptions *options, GlobalConfig *cfg)
{
  if (options->init_window_size == -1)
    options->init_window_size = cfg->log_iw_size;
  g_atomic_counter_set(&options->window_size, options->init_window_size);
}
