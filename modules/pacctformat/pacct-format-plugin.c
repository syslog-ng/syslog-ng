/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 1998-2011 Balázs Scheidler
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

#include "pacct-format.h"
#include "messages.h"
#include "plugin.h"

static MsgFormatHandler *
pacct_format_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  return &pacct_handler;
}

static Plugin pacct_format_plugin =
{
  .type = LL_CONTEXT_FORMAT,
  .name = "pacct",
  .construct = &pacct_format_construct,
};

gboolean
pacctformat_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, &pacct_format_plugin, 1);
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "pacctformat",
  .version = VERSION,
  .description = "The pacctformat module provides support for parsing BSD Process Accounting files",
  .core_revision = SOURCE_REVISION,
  .plugins = &pacct_format_plugin,
  .plugins_len = 1,
};
