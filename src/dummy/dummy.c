/*
 * Copyright (c) 2002-2010 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2010 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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

#include "dummy.h"
#include "dummy-parser.h"
#include "plugin.h"
#include "messages.h"

static gboolean
dummy_dd_init(LogPipe *s)
{
  return TRUE;
}

static gboolean
dummy_dd_deinit(LogPipe *s)
{
  return TRUE;
}

static void
dummy_dd_queue(LogPipe *s, LogMessage *msg, const LogPathOptions *path_options)
{
  DummyDestDriver *self = (DummyDestDriver *) s;
  msg_notice("Dummy plugin received a message",
             evt_tag_str("msg", log_msg_get_value(msg, LM_V_MESSAGE, NULL)),
             evt_tag_int("opt", self->opt),
             NULL);

  log_msg_ack(msg, path_options);
  log_msg_unref(msg);
}

DummyDestDriver *
dummy_dd_new(void)
{
  DummyDestDriver *self = g_new0(DummyDestDriver, 1);

  log_drv_init_instance(&self->super);
  self->super.super.init = dummy_dd_init;
  self->super.super.deinit = dummy_dd_deinit;
  self->super.super.queue = dummy_dd_queue;

  return self;
}

extern CfgParser dummy_dd_parser;

static Plugin dummy_plugin =
{
  .type = LL_CONTEXT_DESTINATION,
  .name = "dummy",
  .parser = &dummy_parser,
};

gboolean
syslogng_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, &dummy_plugin, 1);
  return TRUE;
}
