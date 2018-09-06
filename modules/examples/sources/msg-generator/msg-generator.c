/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 László Várady <laszlo.varady@balabit.com>
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

#include "msg-generator.h"
#include "msg-generator-source.h"

struct MsgGeneratorSourceDriver
{
  LogSrcDriver super;
  MsgGeneratorSourceOptions source_options;
  MsgGeneratorSource *source;
};

static gboolean
msg_generator_sd_init(LogPipe *s)
{
  MsgGeneratorSourceDriver *self = (MsgGeneratorSourceDriver *) s;
  GlobalConfig *cfg = log_pipe_get_config(s);

  if (!log_src_driver_init_method(s))
    return FALSE;

  self->source = msg_generator_source_new(cfg);

  msg_generator_source_options_init(&self->source_options, cfg, self->super.super.group);
  msg_generator_source_set_options(self->source, &self->source_options, self->super.super.id, NULL, FALSE, FALSE,
                                   self->super.super.super.expr_node);

  log_pipe_append((LogPipe *) self->source, &self->super.super.super);

  if (!msg_generator_source_init(self->source))
    {
      msg_generator_source_free(self->source);
      self->source = NULL;
      return FALSE;
    }

  return TRUE;
}

static gboolean
msg_generator_sd_deinit(LogPipe *s)
{
  MsgGeneratorSourceDriver *self = (MsgGeneratorSourceDriver *) s;

  msg_generator_source_deinit(self->source);
  msg_generator_source_free(self->source);
  self->source = NULL;

  return log_src_driver_deinit_method(s);
}

static void
msg_generator_sd_free(LogPipe *s)
{
  MsgGeneratorSourceDriver *self = (MsgGeneratorSourceDriver *) s;

  msg_generator_source_options_destroy(&self->source_options);
  log_src_driver_free(s);
}

MsgGeneratorSourceOptions *
msg_generator_sd_get_source_options(LogDriver *s)
{
  MsgGeneratorSourceDriver *self = (MsgGeneratorSourceDriver *) s;

  return &self->source_options;
}

LogDriver *
msg_generator_sd_new(GlobalConfig *cfg)
{
  MsgGeneratorSourceDriver *self = g_new0(MsgGeneratorSourceDriver, 1);
  log_src_driver_init_instance(&self->super, cfg);

  msg_generator_source_options_defaults(&self->source_options);

  self->super.super.super.init = msg_generator_sd_init;
  self->super.super.super.deinit = msg_generator_sd_deinit;
  self->super.super.super.free_fn = msg_generator_sd_free;

  return &self->super.super;
}
