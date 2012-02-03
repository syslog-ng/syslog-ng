/*
 * Copyright (c) 2012 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2012 Gergely Nagy <algernon@balabit.hu>
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
 */

#include "plugin.h"
#include "templates.h"
#include "cfg.h"
#include "uuid.h"

#include "config.h"

static void
tf_uuid(LogMessage *msg, gint argc, GString *argv[], GString *result)
{
  char uuid_str[37];

  uuid_gen_random(uuid_str, sizeof(uuid_str));
  g_string_append (result, uuid_str);
}

TEMPLATE_FUNCTION_SIMPLE(tf_uuid);

static Plugin tfuuid_plugins[] =
  {
    TEMPLATE_FUNCTION_PLUGIN(tf_uuid, "uuid"),
  };

gboolean
tfuuid_module_init(GlobalConfig *cfg, CfgArgs *args)
{
  plugin_register(cfg, tfuuid_plugins, G_N_ELEMENTS(tfuuid_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "tfuuid",
  .version = VERSION,
  .description = "The tfuuid module provides a template function to generate UUIDs.",
  .core_revision = SOURCE_REVISION,
  .plugins = tfuuid_plugins,
  .plugins_len = G_N_ELEMENTS(tfuuid_plugins),
};
