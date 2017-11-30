/*
 * Copyright (c) 2002-2011 Balabit
 * Copyright (c) 1998-2011 Balázs Scheidler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "confgen.h"
#include "cfg.h"
#include "cfg-block-generator.h"
#include "messages.h"
#include "plugin.h"

#include <string.h>
#include <errno.h>
#include <stdlib.h>


static void
confgen_set_args_as_env(gpointer k, gpointer v, gpointer user_data)
{
  gchar buf[1024];
  g_snprintf(buf, sizeof(buf), "confgen_%s", (gchar *)k);
  setenv(buf, (gchar *)v, 1);
}

static void
confgen_unset_args_from_env(gpointer k, gpointer v, gpointer user_data)
{
  gchar buf[1024];
  g_snprintf(buf, sizeof(buf), "confgen_%s", (gchar *)k);
  unsetenv(buf);
}

typedef struct _ConfgenExec
{
  CfgBlockGenerator super;
  gchar *exec;
} ConfgenExec;

gboolean
confgen_exec_generate(CfgBlockGenerator *s, GlobalConfig *cfg, CfgLexer *lexer, CfgArgs *args)
{
  ConfgenExec *self = (ConfgenExec *) s;
  gchar *value;
  gsize value_len = 0;
  FILE *out;
  gsize res;
  gchar buf[256];
  gboolean result;

  g_snprintf(buf, sizeof(buf), "%s confgen %s", cfg_lexer_lookup_context_name_by_type(self->super.context),
             self->super.name);

  cfg_args_foreach(args, confgen_set_args_as_env, NULL);
  out = popen(self->exec, "r");
  cfg_args_foreach(args, confgen_unset_args_from_env, NULL);

  if (!out)
    {
      msg_error("confgen: Error executing generator program",
                evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(self->super.context)),
                evt_tag_str("block", self->super.name),
                evt_tag_str("exec", self->exec),
                evt_tag_errno("error", errno));
      return FALSE;
    }
  value = g_malloc(1024);
  while ((res = fread(value + value_len, 1, 1024, out)) > 0)
    {
      value_len += res;
      value = g_realloc(value, value_len + 1024);
    }
  res = pclose(out);
  if (res != 0)
    {
      msg_error("confgen: Generator program returned with non-zero exit code",
                evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(self->super.context)),
                evt_tag_str("block", self->super.name),
                evt_tag_str("exec", self->exec),
                evt_tag_int("rc", res));
      g_free(value);
      return FALSE;
    }
  msg_debug("confgen: output from the executed program to be included is",
            evt_tag_printf("block", "%.*s", (gint) value_len, value));
  result = cfg_lexer_include_buffer(lexer, buf, value, value_len);
  g_free(value);
  return result;
}

static void
confgen_exec_free(CfgBlockGenerator *s)
{
  ConfgenExec *self = (ConfgenExec *) s;

  g_free(self->exec);
  cfg_block_generator_free_instance(s);
}

static CfgBlockGenerator *
confgen_exec_new(gint context, const gchar *name, const gchar *exec)
{
  ConfgenExec *self = g_new0(ConfgenExec, 1);

  cfg_block_generator_init_instance(&self->super, context, name);
  self->super.generate = confgen_exec_generate;
  self->super.free_fn = confgen_exec_free;
  self->exec = g_strdup(exec);
  return &self->super;
}

gboolean
confgen_module_init(PluginContext *plugin_context, CfgArgs *args)
{
  const gchar *name, *context, *exec;

  name = cfg_args_get(args, "name");
  if (!name)
    {
      msg_error("confgen: name argument expected");
      return FALSE;
    }
  context = cfg_args_get(args, "context");
  if (!context)
    {
      msg_error("confgen: context argument expected");
      return FALSE;
    }
  exec = cfg_args_get(args, "exec");
  if (!exec)
    {
      msg_error("confgen: exec argument expected");
      return FALSE;
    }
  cfg_lexer_register_generator_plugin(plugin_context, confgen_exec_new(cfg_lexer_lookup_context_type_by_name(context),
                                      name, exec));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "confgen",
  .version = SYSLOG_NG_VERSION,
  .description = "The confgen module provides support for dynamically generated configuration file snippets for syslog-ng, used for the SCL system() driver for example",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = NULL,
  .plugins_len = 0,
};
