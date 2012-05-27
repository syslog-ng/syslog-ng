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

#include "syslog-format.h"
#include "messages.h"
#include "plugin.h"

static MsgFormatHandler syslog_handler =
{
  .parse = &syslog_format_handler
};

static MsgFormatHandler *
syslog_format_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &syslog_handler;
}

static Plugin syslog_format_plugin =
{
  .type = LL_CONTEXT_FORMAT,
  .name = "syslog",
  .construct = (gpointer (*)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)) syslog_format_construct,
};

gboolean
syslogformat_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  syslog_format_init();
  plugin_register(cfg, &syslog_format_plugin, 1);
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "syslogformat",
  .version = VERSION,
  .description = "The syslogformat module provides support for parsing RFC3164 and RFC5424 format syslog messages.",
  .core_revision = SOURCE_REVISION,
  .plugins = &syslog_format_plugin,
  .plugins_len = 1,
};
