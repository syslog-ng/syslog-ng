/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
#include "filterx-config.h"
#include "cfg.h"

#define MODULE_CONFIG_KEY "filterx"

static void
filterx_config_free(ModuleConfig *s)
{
  FilterXConfig *self = (FilterXConfig *) s;

  g_ptr_array_unref(self->frozen_objects);
  module_config_free_method(s);
}

FilterXConfig *
filterx_config_new(GlobalConfig *cfg)
{
  FilterXConfig *self = g_new0(FilterXConfig, 1);

  self->super.free_fn = filterx_config_free;
  self->frozen_objects = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unfreeze_and_free);
  return self;
}

FilterXConfig *
filterx_config_get(GlobalConfig *cfg)
{
  FilterXConfig *fxc = g_hash_table_lookup(cfg->module_config, MODULE_CONFIG_KEY);
  if (!fxc)
    {
      fxc = filterx_config_new(cfg);
      g_hash_table_insert(cfg->module_config, g_strdup(MODULE_CONFIG_KEY), fxc);
    }
  return fxc;
}

FilterXObject *
filterx_config_freeze_object(GlobalConfig *cfg, FilterXObject *object)
{
  FilterXConfig *fxc = filterx_config_get(cfg);
  if (filterx_object_freeze(object))
    g_ptr_array_add(fxc->frozen_objects, object);
  return object;
}
