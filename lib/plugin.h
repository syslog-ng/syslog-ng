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

typedef struct _PluginFailureInfo
{
  gconstpointer aux_data;
} PluginFailureInfo;

typedef struct _PluginBase
{
  /* NOTE: the start of this structure must match the Plugin struct,
     thus it has to be changed together with Plugin */
  gint type;
  gchar *name;
  PluginFailureInfo failure_info;
} PluginBase;

typedef struct _PluginCandidate
{
  PluginBase super;
  gchar *module_name;
} PluginCandidate;

/* A plugin actually registered by a module. See PluginCandidate in
 * the implementation module, which encapsulates a demand-loadable
 * plugin, not yet loaded.
 *
 * A plugin serves as the factory for an extension point (=plugin). In
 * contrast with the "module" which is the shared object itself which
 * registers plugins.  Each module can register a number of plugins,
 * not just one.  */
typedef struct _Plugin Plugin;
struct _Plugin
{
  /* NOTE: the first three fields must match PluginCandidate struct defined
     in the plugin.c module, please modify them both at the same time!  It
     is not referenced as "super" as it would make the using sites having to
     use ugly braces when initializing the structure. */

  gint type;
  const gchar *name;
  PluginFailureInfo failure_info;
  CfgParser *parser;
  gpointer (*construct)(Plugin *self);
  void (*free_fn)(Plugin *s);
};

gpointer plugin_construct(Plugin *self);
gpointer plugin_construct_from_config(Plugin *self, CfgLexer *lexer, gpointer arg);

typedef struct _ModuleInfo ModuleInfo;
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
};

struct _PluginContext
{
  GList *plugins;
  GList *candidate_plugins;
  gchar *module_path;
};

/* instantiate a new plugin */
Plugin *plugin_find(PluginContext *context, gint plugin_type, const gchar *plugin_name);

/* plugin side API */
PluginCandidate *plugin_candidate_new(gint plugin_type, const gchar *name, const gchar *module_name);
void plugin_candidate_free(PluginCandidate *self);

void plugin_register(PluginContext *context, Plugin *p, gint number);
gboolean plugin_load_module(PluginContext *context, const gchar *module_name, CfgArgs *args);
gboolean plugin_is_module_available(PluginContext *context, const gchar *module_name);

void plugin_list_modules(FILE *out, gboolean verbose);

gboolean plugin_has_discovery_run(PluginContext *context);
void plugin_discover_candidate_modules(PluginContext *context);

void plugin_context_copy_candidates(PluginContext *context, PluginContext *src_context);
void plugin_context_set_module_path(PluginContext *context, const gchar *module_path);
void plugin_context_init_instance(PluginContext *context);
void plugin_context_deinit_instance(PluginContext *context);

#endif
