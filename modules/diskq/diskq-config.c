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

#include <math.h>

#include "diskq-config.h"
#include "cfg.h"

#define MODULE_CONFIG_KEY "disk-buffer"
#define DEFAULT_TRUNCATE_SIZE_RATIO_V3_X (0.1)
#define DEFAULT_TRUNCATE_SIZE_RATIO_V4_X (1)
#define DEFAULT_PREALLOC FALSE
#define DEFAULT_STATS_FREQ (300)

static void
disk_queue_config_free(ModuleConfig *s)
{
  module_config_free_method(s);
}

DiskQueueConfig *
disk_queue_config_new(GlobalConfig *cfg)
{
  DiskQueueConfig *self = g_new0(DiskQueueConfig, 1);

  self->truncate_size_ratio = -1;
  self->prealloc = -1;
  self->stats_freq = -1;

  self->super.free_fn = disk_queue_config_free;
  return self;
}

DiskQueueConfig *
disk_queue_config_get(GlobalConfig *cfg)
{
  DiskQueueConfig *dqc = g_hash_table_lookup(cfg->module_config, MODULE_CONFIG_KEY);
  if (!dqc)
    {
      dqc = disk_queue_config_new(cfg);
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

  if (!disk_queue_config_is_truncate_size_ratio_set_explicitly(cfg))
    {
      if (cfg_is_config_version_older(cfg, VERSION_VALUE_4_0))
        return DEFAULT_TRUNCATE_SIZE_RATIO_V3_X;
      return DEFAULT_TRUNCATE_SIZE_RATIO_V4_X;
    }

  return dqc->truncate_size_ratio;
}

gboolean
disk_queue_config_is_truncate_size_ratio_set_explicitly(GlobalConfig *cfg)
{
  DiskQueueConfig *dqc = disk_queue_config_get(cfg);
  return fabs(dqc->truncate_size_ratio - (-1)) >= DBL_EPSILON;
}

void
disk_queue_config_set_prealloc(GlobalConfig *cfg, gboolean prealloc)
{
  DiskQueueConfig *dqc = disk_queue_config_get(cfg);
  dqc->prealloc = prealloc;
}

gboolean
disk_queue_config_get_prealloc(GlobalConfig *cfg)
{
  DiskQueueConfig *dqc = disk_queue_config_get(cfg);

  if (!disk_queue_config_is_prealloc_set_explicitly(cfg))
    return DEFAULT_PREALLOC;

  return dqc->prealloc;
}

gboolean
disk_queue_config_is_prealloc_set_explicitly(GlobalConfig *cfg)
{
  DiskQueueConfig *dqc = disk_queue_config_get(cfg);
  return dqc->prealloc != -1;
}

void
disk_queue_config_set_stats_freq(GlobalConfig *cfg, gint stats_freq)
{
  DiskQueueConfig *dqc = disk_queue_config_get(cfg);
  dqc->stats_freq = stats_freq;
}

gboolean
disk_queue_config_get_stats_freq(GlobalConfig *cfg)
{
  DiskQueueConfig *dqc = disk_queue_config_get(cfg);

  if (!disk_queue_config_is_stats_freq_set_explicitly(cfg))
    return DEFAULT_STATS_FREQ;

  return dqc->stats_freq;
}

gboolean
disk_queue_config_is_stats_freq_set_explicitly(GlobalConfig *cfg)
{
  DiskQueueConfig *dqc = disk_queue_config_get(cfg);
  return dqc->stats_freq != -1;
}
