/*
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Gergely Nagy <algernon@balabit.hu>
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

#include "linux-kmsg-format.h"
#include "messages.h"
#include "plugin.h"
#include "plugin-types.h"

static MsgFormatHandler linux_kmsg_handler =
{
  .parse = &linux_kmsg_format_handler
};

static gpointer
linux_kmsg_format_construct(Plugin *self)
{
  return (gpointer) &linux_kmsg_handler;
}

static Plugin linux_kmsg_format_plugin =
{
  .type = LL_CONTEXT_FORMAT,
  .name = "linux-kmsg",
  .construct = linux_kmsg_format_construct,
};

gboolean
linux_kmsg_format_module_init(PluginContext *context, CfgArgs *args)
{
  linux_msg_format_init();
  plugin_register(context, &linux_kmsg_format_plugin, 1);
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "linux-kmsg-format",
  .version = SYSLOG_NG_VERSION,
  .description = "The linux-kmsg-format module provides support for parsing linux 3.5+ /dev/kmsg-format messages.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = &linux_kmsg_format_plugin,
  .plugins_len = 1,
};
