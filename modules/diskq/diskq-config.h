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

#ifndef DISKQ_CONFIG_H_INCLUDED
#define DISKQ_CONFIG_H_INCLUDED

#include "module-config.h"

typedef struct _DiskQueueConfig
{
  ModuleConfig super;
  gdouble truncate_size_ratio;
} DiskQueueConfig;

DiskQueueConfig *disk_queue_config_get(GlobalConfig *cfg);
void disk_queue_config_set_truncate_size_ratio(GlobalConfig *cfg, gdouble truncate_size_ratio);
gdouble disk_queue_config_get_truncate_size_ratio(GlobalConfig *cfg);

#endif

