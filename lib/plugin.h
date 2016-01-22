/*
 * Copyright (c) 2002-2012 Balabit
 * Copyright (c) 1998-2012 Balázs Scheidler
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

#ifndef PLUGIN_H_INCLUDED
#define PLUGIN_H_INCLUDED

#include "cfg-parser.h"

typedef struct _Plugin Plugin;
typedef struct _ModuleInfo ModuleInfo;

/* A plugin actually registered by a module. See PluginCandidate in
 * the implementation module, which encapsulates a demand-loadable
 * plugin, not yet loaded.
 *
 * A plugin serves as the factory for an extension point (=plugin). In
 * contrast with the "module" which is the shared object itself which
 * registers plugins.  Each module can register a number of plugins,
 * not just one.  */
struct _Plugin
{
  /* NOTE: the first two fields must match PluginCandidate struct defined in
     the plugin.c module, please modify them both at the same time! */
  gint type;
  const gchar *name;
  CfgParser *parser;
  void (*setup_context)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name);
  gpointer (*construct)(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name);
  void (*free_fn)(Plugin *s);
};

struct _ModuleInfo
{
  /* name of the module to be loaded as */
  const gchar *canonical_name;
  /* version number if any */
  const gchar *version;
  /* copyright information, homepage and other important information about the module */
  const gchar *description;
  /* git sha that identifies the core revision that this was compiled for */
  const gchar *core_revision;
  Plugin *plugins;
  gint plugins_len;
  /* the higher the better */
  gint preference;
};

/* instantiate a new plugin */
Plugin *plugin_find(GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name);
gpointer plugin_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name);
gpointer plugin_parse_config(Plugin *plugin, GlobalConfig *cfg, YYLTYPE *yylloc, gpointer arg);


/* plugin side API */

void plugin_register(GlobalConfig *cfg, Plugin *p, gint number);
gboolean plugin_load_module(const gchar *module_name, GlobalConfig *cfg, CfgArgs *args);

void plugin_list_modules(FILE *out, gboolean verbose);

void plugin_load_candidate_modules(GlobalConfig *cfg);
void plugin_free_candidate_modules(GlobalConfig *cfg);
void plugin_free_plugins(GlobalConfig *cfg);

extern const gchar *initial_module_path;

void plugin_global_init(void);
void plugin_global_deinit(void);

#endif
