/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
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
#include "python-config.h"
#include "python-main.h"
#include "cfg.h"

#define MODULE_CONFIG_KEY "python"

static gboolean
python_config_init(ModuleConfig *s, GlobalConfig *cfg)
{
  PythonConfig *self = (PythonConfig *) s;

  propagate_persist_state(cfg);

  PyGILState_STATE gstate;
  gstate = PyGILState_Ensure();
  _py_switch_main_module(self);
  PyGILState_Release(gstate);
  return TRUE;
}

static void
python_config_free(ModuleConfig *s)
{
  PythonConfig *self = (PythonConfig *) s;
  PyGILState_STATE gstate;

  gstate = PyGILState_Ensure();
  Py_XDECREF(self->main_module);
  PyGILState_Release(gstate);
  module_config_free_method(s);
}

PythonConfig *
python_config_new(void)
{
  PythonConfig *self = g_new0(PythonConfig, 1);

  self->super.init = python_config_init;
  self->super.free_fn = python_config_free;
  return self;
}

PythonConfig *
python_config_get(GlobalConfig *cfg)
{
  PythonConfig *pc = g_hash_table_lookup(cfg->module_config, MODULE_CONFIG_KEY);
  if (!pc)
    {
      pc = python_config_new();
      g_hash_table_insert(cfg->module_config, g_strdup(MODULE_CONFIG_KEY), pc);
    }
  return pc;
}
