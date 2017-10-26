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

#include "messages.h"
#include "cfg-parser.h"
#include "plugin.h"
#include "plugin-types.h"
#include "journald-subsystem.h"

extern CfgParser systemd_journal_parser;

static Plugin systemd_journal_plugins[] =
{
  {
    .type = LL_CONTEXT_SOURCE,
    .name = "systemd-journal",
    .parser = &systemd_journal_parser,
  },
};

gboolean
systemd_journal_module_init(PluginContext *context, CfgArgs *args)
{
  if (!load_journald_subsystem())
    {
      msg_error("Can't find systemd-journal on this system");
      return FALSE;
    }
  plugin_register(context, systemd_journal_plugins, G_N_ELEMENTS(systemd_journal_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "systemd-journal",
  .version = SYSLOG_NG_VERSION,
  .description = "The systemd-journal module provides systemd journal source drivers for syslog-ng where it is available.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = systemd_journal_plugins,
  .plugins_len = G_N_ELEMENTS(systemd_journal_plugins),
};
