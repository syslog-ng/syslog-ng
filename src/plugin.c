#include "plugin.h"
#include "driver.h"
#include "messages.h"
#include "cfg-lexer.h"

#include <gmodule.h>

void
plugin_register(GlobalConfig *cfg, Plugin *p, gint number)
{
  gint i;

  for (i = 0; i < number; i++)
    {
      if (plugin_find(cfg, p[i].type, p[i].name))
        {
          msg_debug("Attempted to register the same plugin multiple times, ignoring",
                    evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(p[i].type)),
                    evt_tag_str("name", p[i].name),
                    NULL);
          continue;
        }
      cfg->plugins = g_list_prepend(cfg->plugins, &p[i]);
    }
}

Plugin *
plugin_find(GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  GList *p;
  Plugin *plugin;

  for (p = cfg->plugins; p; p = g_list_next(p))
    {
      plugin = p->data;
      if (plugin->type == plugin_type && strcmp(plugin->name, plugin_name) == 0)
        return plugin;
    }
  return NULL;
}

/* construct a plugin without having a configuration file to parse */
gpointer
plugin_construct(Plugin *self, GlobalConfig *cfg, gint plugin_type, const gchar *plugin_name)
{
  g_assert(self->parser == NULL);
  if (self->construct)
    {
      return self->construct(self, cfg, plugin_type, plugin_name);
    }
  return NULL;
}

/* construct a plugin instance by parsing a configuration file */
gpointer
plugin_parse_config(Plugin *self, GlobalConfig *cfg, YYLTYPE *yylloc)
{
  gpointer instance = NULL;

  g_assert(self->construct == NULL);

  /* make sure '_' and '-' are handled equally in plugin name */
  if (!self->setup_context)
    {
      CfgTokenBlock *block;
      YYSTYPE token;

      block = cfg_token_block_new();

      memset(&token, 0, sizeof(token));
      token.type = LL_TOKEN;
      token.token = self->type;
      cfg_token_block_add_token(block, &token);
      cfg_lexer_push_context(cfg->lexer, self->parser->context, self->parser->keywords, self->parser->name);
      cfg_lexer_lookup_keyword(cfg->lexer, &token, yylloc, self->name);
      cfg_lexer_pop_context(cfg->lexer);
      cfg_token_block_add_token(block, &token);

      cfg_lexer_inject_token_block(cfg->lexer, block);
    }
  else
    {
      (self->setup_context)(self, cfg, self->type, self->name);
    }

  if (!cfg_parser_parse(self->parser, cfg->lexer, &instance))
    {
      cfg_parser_cleanup(self->parser, instance);
      instance = NULL;
    }

  return instance;
}

gboolean
plugin_load_module(const gchar *module_name, GlobalConfig *cfg, CfgArgs *args)
{
  GModule *mod;
  gboolean (*init_func)(GlobalConfig *cfg, CfgArgs *args);
  gchar *plugin_module_name = NULL;
  const gchar *module_path;
  gchar **module_path_dirs;
  gint i;

  module_path = cfg_args_get(cfg->lexer->globals, "module-path");
  if (!module_path)
    module_path = PATH_PLUGINDIR;

  module_path_dirs = g_strsplit(module_path, G_SEARCHPATH_SEPARATOR_S, 0);
  i = 0;
  while (module_path_dirs && module_path_dirs[i])
    {
      plugin_module_name = g_module_build_path(module_path_dirs[i], module_name);
      if (g_file_test(plugin_module_name, G_FILE_TEST_EXISTS))
        break;
      g_free(plugin_module_name);
      plugin_module_name = NULL;
      i++;
    }
  g_strfreev(module_path_dirs);
  if (!plugin_module_name)
    {
      msg_error("Plugin module not found in 'module-path'",
                evt_tag_str("module-path", module_path),
                evt_tag_str("module", module_name),
                NULL);
      return FALSE;
    }
  mod = g_module_open(plugin_module_name, G_MODULE_BIND_LOCAL);
  g_free(plugin_module_name);
  if (!mod)
    {
      msg_error("Error opening plugin module",
                evt_tag_str("module", module_name),
                evt_tag_str("error", g_module_error()),
                NULL);
      return FALSE;
    }
  g_module_make_resident(mod);
  if (!g_module_symbol(mod, "syslogng_module_init", (gpointer *) &init_func))
    {
      msg_error("Error finding init function in module",
                evt_tag_str("module", module_name),
                evt_tag_str("symbol", "syslogng_module_init"),
                evt_tag_str("error", g_module_error()),
                NULL);
      return FALSE;
    }
  return (*init_func)(cfg, args);
}
