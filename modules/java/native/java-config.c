/*
 * Copyright (c) 2018 Balabit
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
#include "java-config.h"
#include "cfg.h"

#define MODULE_CONFIG_KEY "java"

static gboolean
java_config_init(ModuleConfig *s, GlobalConfig *cfg)
{
  return TRUE;
}

static void
java_config_free(ModuleConfig *s)
{
  JavaConfig *self = (JavaConfig *) s;

  if (self->jvm_options)
    g_free(self->jvm_options);
  self->jvm_options = NULL;

  module_config_free_method(s);
}

JavaConfig *
java_config_new(void)
{
  JavaConfig *self = g_new0(JavaConfig, 1);

  self->super.init = java_config_init;
  self->super.free_fn = java_config_free;
  return self;
}

JavaConfig *
java_config_get(GlobalConfig *cfg)
{
  JavaConfig *jc = g_hash_table_lookup(cfg->module_config, MODULE_CONFIG_KEY);
  if (!jc)
    {
      jc = java_config_new();
      g_hash_table_insert(cfg->module_config, g_strdup(MODULE_CONFIG_KEY), jc);
    }
  return jc;
}

void
java_config_set_jvm_options(GlobalConfig *cfg, gchar *jvm_options)
{
  JavaConfig *jc = java_config_get(cfg);
  jc -> jvm_options = jvm_options;
}

gchar *
java_config_get_jvm_options(GlobalConfig *cfg)
{
  JavaConfig *jc = java_config_get(cfg);
  return (jc -> jvm_options);
}
