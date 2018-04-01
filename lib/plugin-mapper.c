/*
 * Copyright (c) 2018 Balabit
 * Copyright (c) 2018 Kokan
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

#include "plugin-mapper.h"

static void
_free_fn(PluginMapper *s)
{
  SimplePluginMapper *self = (SimplePluginMapper *)s;

  g_hash_table_destroy(self->map);
  g_free(self);
}

static gchar *
_map(PluginMapper *s, const gint plugin_type, const gchar *plugin_name)
{
  SimplePluginMapper *self = (SimplePluginMapper *)s;

  gchar *new_name = (gchar *)g_hash_table_lookup(self->map, plugin_name);

  return new_name;
}

PluginMapper *simple_plugin_mapper_new()
{
  SimplePluginMapper *self = g_new0(SimplePluginMapper,1);

  self->map = g_hash_table_new(g_str_hash, g_str_equal);
  self->super.free_fn = _free_fn;
  self->super.map     = _map;

  return (PluginMapper *)self;
}

void
simple_plugin_mapper_add_alternative_name(SimplePluginMapper *self, gchar *new_name, gchar *original_name)
{
  g_hash_table_insert(self->map, new_name, original_name);
}

PluginMapper *hu_plugin_mapper_new()
{
  HuPluginMapper *self = simple_plugin_mapper_new();

  simple_plugin_mapper_add_alternative_name(self, "fájl", "file");
  simple_plugin_mapper_add_alternative_name(self, "rendszerd-napló", "systemd-journal");
  simple_plugin_mapper_add_alternative_name(self, "rugalmaskeresés2", "elasticsearch2");
  simple_plugin_mapper_add_alternative_name(self, "hálózat", "network");

  return (PluginMapper *)self;
}
