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

#include "plugin.h"
#include "plugin-types.h"
#include "messages.h"
#include "pathutils.h"
#include "resolved-configurable-paths.h"

#include <gmodule.h>
#include <string.h>

#ifdef _AIX
#undef G_MODULE_SUFFIX
#define G_MODULE_SUFFIX "a"
#endif

#ifdef __APPLE__
#undef G_MODULE_SUFFIX
#define G_MODULE_SUFFIX "dylib"
#endif

static void
plugin_candidate_set_module_name(PluginCandidate *self, const gchar *module_name)
{
  g_free(self->module_name);
  self->module_name = g_strdup(module_name);
}


PluginCandidate *
plugin_candidate_new(gint plugin_type, const gchar *name, const gchar *module_name)
{
  PluginCandidate *self = g_new0(PluginCandidate, 1);

  self->super.type = plugin_type;
  self->super.name = g_strdup(name);
  self->module_name = g_strdup(module_name);
  self->super.failure_info.aux_data = NULL;

  return self;
}

void
plugin_candidate_free(PluginCandidate *self)
{
  g_free(self->super.name);
  g_free(self->module_name);
  g_free(self);
}

/* construct a plugin without having a configuration file to parse */
gpointer
plugin_construct(Plugin *self)
{
  if (self->construct)
    {
      return self->construct(self);
    }
  return NULL;
}

static void
plugin_free(Plugin *plugin)
{
  if (plugin->free_fn)
    plugin->free_fn(plugin);
}

static gboolean
_is_log_pipe(Plugin *self)
{
  switch (self->type)
    {
    case LL_CONTEXT_SOURCE:
    case LL_CONTEXT_DESTINATION:
    case LL_CONTEXT_PARSER:
    case LL_CONTEXT_REWRITE:
      return TRUE;
    default:
      return FALSE;
    }
}

gpointer
plugin_construct_from_config(Plugin *self, CfgLexer *lexer, gpointer arg)
{
  gpointer instance = NULL;

  g_assert(self->construct == NULL);
  if (!cfg_parser_parse(self->parser, lexer, &instance, arg))
    {
      cfg_parser_cleanup(self->parser, instance);
      return NULL;
    }

  if (_is_log_pipe(self))
    {
      LogPipe *p = (LogPipe *)instance;
      p->plugin_name = g_strdup(self->name);
      if (self->failure_info.aux_data != NULL)
        p->init = self->failure_info.aux_data;
    }

  return instance;
}

/*****************************************************************************
 * Implementation of PluginContext
 *****************************************************************************/

static Plugin *
_find_plugin_in_list(GList *head, gint plugin_type, const gchar *plugin_name)
{
  GList *p;
  Plugin *plugin;
  gint i;

  /* this function can only use the first two fields in plugin (type &
   * name), because it may be supplied a list of PluginCandidate
   * instances too */

  for (p = head; p; p = g_list_next(p))
    {
      plugin = p->data;
      if (plugin->type == plugin_type)
        {
          for (i = 0; plugin->name[i] && plugin_name[i]; i++)
            {
              if (plugin->name[i] != plugin_name[i] &&
                  !((plugin->name[i] == '-' || plugin->name[i] == '_') &&
                    (plugin_name[i] == '-' || plugin_name[i] == '_')))
                {
                  break;
                }
            }
          if (plugin_name[i] == 0 && plugin->name[i] == 0)
            return plugin;
        }
    }
  return NULL;
}

static ModuleInfo *
_get_module_info(GModule *mod)
{
  ModuleInfo *module_info = NULL;

  if (mod && g_module_symbol(mod, "module_info", (gpointer *) &module_info))
    return module_info;
  return NULL;
}

static gchar *
_format_module_init_name(const gchar *module_name)
{
  gchar *module_init_func;
  gchar *p;

  module_init_func = g_strdup_printf("%s_module_init", module_name);
  for (p = module_init_func; *p; p++)
    {
      if ((*p) == '-')
        *p = '_';
    }
  return module_init_func;
}

static GModule *
_dlopen_module_as_filename(const gchar *module_file_name, const gchar *module_name)
{
  GModule *mod = NULL;

  msg_trace("Trying to open module",
            evt_tag_str("module", module_name),
            evt_tag_str("filename", module_file_name));

  mod = g_module_open(module_file_name, G_MODULE_BIND_LAZY);
  if (!mod)
    {
      msg_info("Error opening plugin module",
               evt_tag_str("module", module_name),
               evt_tag_str("error", g_module_error()));
      return NULL;
    }
  return mod;
}

static GModule *
_dlopen_module_as_dir_and_filename(const gchar *module_dir_name, const gchar *module_file_name,
                                   const gchar *module_name)
{
  gchar *path;
  GModule *mod;

  path = g_build_path(G_DIR_SEPARATOR_S, module_dir_name, module_file_name, NULL);
  mod = _dlopen_module_as_filename(path, module_name);
  g_free(path);
  return mod;
}

static GModule *
_dlopen_module_on_path(const gchar *module_name, const gchar *module_path)
{
  gchar *plugin_module_name = NULL;
  gchar **module_path_dirs, *p, *dot;
  GModule *mod;
  gint i;

  module_path_dirs = g_strsplit(module_path ? : "", G_SEARCHPATH_SEPARATOR_S, 0);
  i = 0;
  while (module_path_dirs && module_path_dirs[i])
    {
      plugin_module_name = g_module_build_path(module_path_dirs[i], module_name);
      if (is_file_regular(plugin_module_name))
        break;

      /* also check if a libtool archive exists (for example in the build directory) */
#ifndef _AIX
      dot = strrchr(plugin_module_name, '.');
      if (dot)
        {
          *dot = 0;
          p = g_strdup_printf("%s.la", plugin_module_name);
          g_free(plugin_module_name);
          plugin_module_name = p;
        }
      if (is_file_regular(plugin_module_name))
        break;

      /* On AIX the modules in .a files */
#else
      dot = strrchr(plugin_module_name, '.');
      if (dot)
        {
          *dot = 0;
          p = g_strdup_printf("%s.a", plugin_module_name);
          g_free(plugin_module_name);
          plugin_module_name = p;
        }
      if (is_file_regular(plugin_module_name))
        break;
#endif
#ifdef __APPLE__
      /* On Mac the modules are in .dylib files */
      dot = strrchr(plugin_module_name, '.');
      if (dot)
        {
          *dot = 0;
          p = g_strdup_printf("%s.dylib", plugin_module_name);
          g_free(plugin_module_name);
          plugin_module_name = p;
        }
      if (is_file_regular(plugin_module_name))
        break;
#endif

      g_free(plugin_module_name);
      plugin_module_name = NULL;
      i++;
    }
  g_strfreev(module_path_dirs);
  if (!plugin_module_name)
    {
      msg_error("Plugin module not found in 'module-path'",
                evt_tag_str("module-path", module_path),
                evt_tag_str("module", module_name));
      return NULL;
    }

  mod = _dlopen_module_as_filename(plugin_module_name, module_name);
  g_free(plugin_module_name);
  return mod;
}

void
plugin_register(PluginContext *context, Plugin *p, gint number)
{
  gint i;

  for (i = 0; i < number; i++)
    {
      Plugin *existing_plugin;

      existing_plugin = _find_plugin_in_list(context->plugins, p[i].type, p[i].name);
      if (existing_plugin)
        {
          msg_debug("Attempted to register the same plugin multiple times, dropping the old one",
                    evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(p[i].type)),
                    evt_tag_str("name", p[i].name));
          plugin_free(existing_plugin);
          context->plugins = g_list_remove(context->plugins, existing_plugin);
        }
      context->plugins = g_list_prepend(context->plugins, &p[i]);
    }
}

Plugin *
plugin_find(PluginContext *context, gint plugin_type, const gchar *plugin_name)
{
  Plugin *p;
  PluginCandidate *candidate;

  /* try registered plugins first */
  p = _find_plugin_in_list(context->plugins, plugin_type, plugin_name);
  if (p)
    {
      return p;
    }

  candidate = (PluginCandidate *) _find_plugin_in_list(context->candidate_plugins, plugin_type, plugin_name);
  if (!candidate)
    return NULL;

  /* try to autoload the module */
  plugin_load_module(context, candidate->module_name, NULL);

  /* by this time it should've registered */
  p = _find_plugin_in_list(context->plugins, plugin_type, plugin_name);
  if (p)
    {
      p->failure_info.aux_data = candidate->super.failure_info.aux_data;
      return p;
    }
  else
    {
      msg_error("This module claims to support a plugin, which it didn't register after loading",
                evt_tag_str("module", candidate->module_name),
                evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(plugin_type)),
                evt_tag_str("name", plugin_name));
    }
  return NULL;
}

gboolean
plugin_load_module(PluginContext *context, const gchar *module_name, CfgArgs *args)
{
  GModule *mod;
  static GModule *main_module_handle;
  gboolean (*init_func)(PluginContext *context, CfgArgs *args);
  gchar *module_init_func;
  gboolean result;
  ModuleInfo *module_info;

  /* lookup in the main executable */
  if (!main_module_handle)
    main_module_handle = g_module_open(NULL, 0);
  module_init_func = _format_module_init_name(module_name);

  if (g_module_symbol(main_module_handle, module_init_func, (gpointer *) &init_func))
    {
      /* already linked in, no need to load explicitly */
      goto call_init;
    }

  /* try to load it from external .so */
  mod = _dlopen_module_on_path(module_name, context->module_path);
  if (!mod)
    {
      g_free(module_init_func);
      return FALSE;
    }
  g_module_make_resident(mod);
  module_info = _get_module_info(mod);

  if (module_info->canonical_name)
    {
      g_free(module_init_func);
      module_init_func = _format_module_init_name(module_info->canonical_name ? : module_name);
    }

  if (!g_module_symbol(mod, module_init_func, (gpointer *) &init_func))
    {
      msg_error("Error finding init function in module",
                evt_tag_str("module", module_name),
                evt_tag_str("symbol", module_init_func),
                evt_tag_str("error", g_module_error()));
      g_free(module_init_func);
      return FALSE;
    }

call_init:
  g_free(module_init_func);
  result = (*init_func)(context, args);
  if (result)
    msg_verbose("Module loaded and initialized successfully",
                evt_tag_str("module", module_name));
  else
    msg_error("Module initialization failed",
              evt_tag_str("module", module_name));
  return result;
}

gboolean
plugin_is_module_available(PluginContext *context, const gchar *module_name)
{
  for (GList *l = context->candidate_plugins; l; l = l->next)
    {
      PluginCandidate *pc = (PluginCandidate *) l->data;

      if (strcmp(pc->module_name, module_name) == 0)
        return TRUE;
    }
  return FALSE;
}

gboolean
plugin_is_plugin_available(PluginContext *context, gint plugin_type, const gchar *plugin_name)
{
  Plugin *p;
  PluginCandidate *candidate;

  p = _find_plugin_in_list(context->plugins, plugin_type, plugin_name);
  if (p)
    return TRUE;

  candidate = (PluginCandidate *) _find_plugin_in_list(context->candidate_plugins, plugin_type, plugin_name);
  return !!candidate;
}


/************************************************************
 * Candidate modules
 ************************************************************/

static void
_free_candidate_plugins(PluginContext *context)
{
  g_list_foreach(context->candidate_plugins, (GFunc) plugin_candidate_free, NULL);
  g_list_free(context->candidate_plugins);
  context->candidate_plugins = NULL;
}

gboolean
plugin_has_discovery_run(PluginContext *context)
{
  return context->candidate_plugins != NULL;
}

void
plugin_discover_candidate_modules(PluginContext *context)
{
  GModule *mod;
  gchar **mod_paths;
  gint i, j;

  _free_candidate_plugins(context);

  mod_paths = g_strsplit(context->module_path ? : "", G_SEARCHPATH_SEPARATOR_S, 0);
  for (i = 0; mod_paths[i]; i++)
    {
      GDir *dir;
      const gchar *fname;

      msg_debug("Reading path for candidate modules",
                evt_tag_str("path", mod_paths[i]));
      dir = g_dir_open(mod_paths[i], 0, NULL);
      if (!dir)
        continue;
      while ((fname = g_dir_read_name(dir)))
        {
          if (g_str_has_suffix(fname, G_MODULE_SUFFIX))
            {
              gchar *module_name;
              ModuleInfo *module_info;
              const gchar *so_basename = fname;

              if (g_str_has_prefix(fname, "lib"))
                so_basename = fname + 3;
              module_name = g_strndup(so_basename, (gint) (strlen(so_basename) - strlen(G_MODULE_SUFFIX) - 1));

              msg_debug("Reading shared object for a candidate module",
                        evt_tag_str("path", mod_paths[i]),
                        evt_tag_str("fname", fname),
                        evt_tag_str("module", module_name));
              mod = _dlopen_module_as_dir_and_filename(mod_paths[i], fname, module_name);
              module_info = _get_module_info(mod);

              if (module_info)
                {
                  for (j = 0; j < module_info->plugins_len; j++)
                    {
                      Plugin *plugin = &module_info->plugins[j];
                      PluginCandidate *candidate_plugin;

                      candidate_plugin = (PluginCandidate *) _find_plugin_in_list(context->candidate_plugins, plugin->type, plugin->name);

                      msg_debug("Registering candidate plugin",
                                evt_tag_str("module", module_name),
                                evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(plugin->type)),
                                evt_tag_str("name", plugin->name));
                      if (candidate_plugin)
                        {
                          msg_debug("Duplicate plugin candidate, overriding previous registration with the new one",
                                    evt_tag_str("old-module", candidate_plugin->module_name),
                                    evt_tag_str("new-module", module_name),
                                    evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(plugin->type)),
                                    evt_tag_str("name", plugin->name));
                          plugin_candidate_set_module_name(candidate_plugin, module_name);
                        }
                      else
                        {
                          context->candidate_plugins = g_list_prepend(context->candidate_plugins,
                                                                      plugin_candidate_new(plugin->type, plugin->name,
                                                                          module_name));
                        }
                    }
                }
              g_free(module_name);
              if (mod)
                g_module_close(mod);
              else
                mod = NULL;
            }
        }
      g_dir_close(dir);
    }
  g_strfreev(mod_paths);
}


static void
_free_plugins(PluginContext *context)
{
  g_list_foreach(context->plugins, (GFunc) plugin_free, NULL);
  g_list_free(context->plugins);
  context->plugins = NULL;
}

void
plugin_context_copy_candidates(PluginContext *context, PluginContext *from)
{
  GList *l;

  _free_candidate_plugins(context);
  for (l = from->candidate_plugins; l; l = l->next)
    {
      PluginCandidate *pc = (PluginCandidate *) l->data;

      context->candidate_plugins =
        g_list_prepend(context->candidate_plugins,
                       plugin_candidate_new(pc->super.type, pc->super.name, pc->module_name));
    }
}

void
plugin_context_set_module_path(PluginContext *context, const gchar *module_path)
{
  g_free(context->module_path);
  context->module_path = g_strdup(module_path);
}

void
plugin_context_init_instance(PluginContext *context)
{
  memset(context, 0, sizeof(*context));
  plugin_context_set_module_path(context, resolved_configurable_paths.initial_module_path);
}

void
plugin_context_deinit_instance(PluginContext *context)
{
  _free_plugins(context);
  _free_candidate_plugins(context);

  g_free(context->module_path);
}

/* global functions */

void
plugin_list_modules(FILE *out, gboolean verbose)
{
  GModule *mod;
  gchar **mod_paths;
  gint i, j, k;
  gboolean first = TRUE;

  mod_paths = g_strsplit(resolved_configurable_paths.initial_module_path, ":", 0);
  for (i = 0; mod_paths[i]; i++)
    {
      GDir *dir;
      const gchar *fname;

      dir = g_dir_open(mod_paths[i], 0, NULL);
      if (!dir)
        continue;
      while ((fname = g_dir_read_name(dir)))
        {
          if (g_str_has_suffix(fname, G_MODULE_SUFFIX))
            {
              gchar *module_name;
              ModuleInfo *module_info;
              const gchar *so_basename = fname;

              if (g_str_has_prefix(fname, "lib"))
                so_basename = fname + 3;
              module_name = g_strndup(so_basename, (gint) (strlen(so_basename) - strlen(G_MODULE_SUFFIX) - 1));

              mod = _dlopen_module_as_dir_and_filename(mod_paths[i], fname, module_name);
              module_info = _get_module_info(mod);
              if (verbose)
                {
                  fprintf(out, "Module: %s\n", module_name);
                  if (mod)
                    {
                      if (!module_info)
                        {
                          fprintf(out, "Status: Unable to resolve module_info variable, probably not a syslog-ng module\n");
                        }
                      else
                        {
                          gchar **lines;

                          fprintf(out, "Status: ok\n"
                                  "Version: %s\n"
                                  "Core-Revision: %s\n"
                                  "Description:\n", module_info->version, module_info->core_revision);

                          lines = g_strsplit(module_info->description, "\n", 0);
                          for (k = 0; lines[k]; k++)
                            fprintf(out, "  %s\n", lines[k][0] ? lines[k] : ".");
                          g_strfreev(lines);

                          fprintf(out, "Plugins:\n");
                          for (j = 0; j < module_info->plugins_len; j++)
                            {
                              Plugin *plugin = &module_info->plugins[j];

                              fprintf(out, "  %-15s %s\n", cfg_lexer_lookup_context_name_by_type(plugin->type), plugin->name);
                            }
                        }
                    }
                  else
                    {
                      fprintf(out, "Status: Unable to dlopen shared object, probably not a syslog-ng module\n");
                    }
                  fprintf(out, "\n");
                }
              else if (module_info)
                {
                  fprintf(out, "%s%s", first ? "" : ",", module_name);
                  first = FALSE;
                }
              g_free(module_name);
              if (mod)
                g_module_close(mod);
            }
        }
      g_dir_close(dir);
    }
  g_strfreev(mod_paths);
  if (!verbose)
    fprintf(out, "\n");
}
