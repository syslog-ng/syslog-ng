/*
 * Copyright (c) 2021 One Identity
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

#include "diskq-config.h"
#include "cfg.h"

#define MODULE_CONFIG_KEY "disk-buffer"

static void
disk_queue_config_free(ModuleConfig *s)
{
  module_config_free_method(s);
}

static void
_set_default_values(DiskQueueConfig *self)
{
  self->truncate_size_ratio = 0.1;
}

DiskQueueConfig *
disk_queue_config_new(void)
{
  DiskQueueConfig *self = g_new0(DiskQueueConfig, 1);

  self->super.free_fn = disk_queue_config_free;
  _set_default_values(self);
  return self;
}

DiskQueueConfig *
disk_queue_config_get(GlobalConfig *cfg)
{
  DiskQueueConfig *dqc = g_hash_table_lookup(cfg->module_config, MODULE_CONFIG_KEY);
  if (!dqc)
    {
      dqc = disk_queue_config_new();
      g_hash_table_insert(cfg->module_config, g_strdup(MODULE_CONFIG_KEY), dqc);
    }
  return dqc;
}

void
disk_queue_config_set_truncate_size_ratio(GlobalConfig *cfg, gdouble truncate_size_ratio)
{
  DiskQueueConfig *dqc = disk_queue_config_get(cfg);
  dqc->truncate_size_ratio = truncate_size_ratio;
}

gdouble
disk_queue_config_get_truncate_size_ratio(GlobalConfig *cfg)
{
  DiskQueueConfig *dqc = disk_queue_config_get(cfg);
  return dqc->truncate_size_ratio;
}
