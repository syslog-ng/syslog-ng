#include "plugin.h"
#include "driver.h"
#include "messages.h"

#include <gmodule.h>

GList *plugins;

void
plugin_register(Plugin *p, gint number)
{
  gint i;

  for (i = 0; i < number; i++)
    plugins = g_list_prepend(plugins, &p[i]);
}

static Plugin *
plugin_find(gint plugin_type, const gchar *plugin_name)
{
  GList *p;
  Plugin *plugin;

  for (p = plugins; p; p = g_list_next(p))
    {
      plugin = p->data;
      if (plugin->type == plugin_type && strcmp(plugin->name, plugin_name) == 0)
        return plugin;
    }
  return NULL;
}

gpointer
plugin_new_instance(CfgLexer *lexer, gint plugin_type, const gchar *plugin_name)
{
  Plugin *p;
  gpointer instance = NULL;

  /* make sure '_' and '-' are handled equally in plugin name */
  p = plugin_find(plugin_type, plugin_name);
  if (!p)
    return NULL;

  if (!p->setup_context)
    {
      CfgTokenBlock *block;
      YYSTYPE token;

      block = cfg_token_block_new();

      memset(&token, 0, sizeof(token));
      token.type = LL_TOKEN;
      token.token = plugin_type;
      cfg_token_block_add_token(block, &token);
      cfg_lexer_add_token_block(lexer, block);
      cfg_lexer_unput_string(lexer, plugin_name);
    }
  else
    {
      (p->setup_context)(p, lexer, plugin_type, plugin_name);
    }

  if (!cfg_parser_parse(p->parser, lexer, &instance))
    {
      cfg_parser_cleanup(p->parser, instance);
      instance = NULL;
    }

  return instance;
}

gboolean
plugin_load_module(const gchar *module_name)
{
  GModule *mod;
  gboolean (*init_func)(void);
  gchar *plugin_module_name;

  plugin_module_name = g_module_build_path(PATH_PLUGINDIR, module_name);
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
  return (*init_func)();
}
