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

#ifndef PLUGIN_MAPPAR_H_INCLUDED
#define PLUGIN_MAPPAR_H_INCLUDED

#include "compat/glib.h"

typedef struct _PluginMapper PluginMapper;

struct _PluginMapper
{
  gchar *(*map)(PluginMapper *self, const gint plugin_type, const gchar *plugin_name);
  void (*free_fn)(PluginMapper *self);
};

static inline gchar *
plugin_mapper_map(PluginMapper *self, const gint plugin_type, const gchar *plugin_name)
{
  if (self->map)
    return self->map(self, plugin_type, plugin_name);

  return g_strdup(plugin_name);
};

static inline void
plugin_mapper_free(PluginMapper *self)
{
  if (self->free_fn)
    self->free_fn(self);
  else
    g_assert_not_reached();
};

typedef struct _SimplePluginMapper
{
  PluginMapper super;
  GHashTable *map;
} SimplePluginMapper;

PluginMapper *simple_plugin_mapper_new();

void simple_plugin_mapper_add_alternative_name(SimplePluginMapper *self, gchar *new_name, gchar *original_name);

typedef SimplePluginMapper HuPluginMapper;

PluginMapper *hu_plugin_mapper_new();

#endif
