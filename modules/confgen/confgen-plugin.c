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

static void
_read_program_output(FILE *out, GString *result)
{
  gchar buf[1024];
  gsize res;

  while ((res = fread(buf, 1, sizeof(buf), out)) > 0)
    {
      g_string_append_len(result, buf, res);
    }
}

gboolean
confgen_exec_generate(CfgBlockGenerator *s, GlobalConfig *cfg, CfgArgs *args, GString *result, const gchar *reference)
{
  ConfgenExec *self = (ConfgenExec *) s;
  FILE *out;
  gchar buf[256];
  gint res;

  g_snprintf(buf, sizeof(buf), "%s confgen %s", cfg_lexer_lookup_context_name_by_type(self->super.context),
             self->super.name);

  cfg_args_foreach(args, confgen_set_args_as_env, NULL);
  out = popen(self->exec, "r");
  cfg_args_foreach(args, confgen_unset_args_from_env, NULL);

  if (!out)
    {
      msg_error("confgen: Error executing generator program",
                evt_tag_str("reference", reference),
                evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(self->super.context)),
                evt_tag_str("block", self->super.name),
                evt_tag_str("exec", self->exec),
                evt_tag_error("error"));
      return FALSE;
    }

  _read_program_output(out, result);
  res = pclose(out);
  if (res != 0)
    {
      msg_error("confgen: Generator program returned with non-zero exit code",
                evt_tag_str("reference", reference),
                evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(self->super.context)),
                evt_tag_str("block", self->super.name),
                evt_tag_str("exec", self->exec),
                evt_tag_int("rc", res));
      return FALSE;
    }
  msg_debug("confgen: output from the executed program to be included is",
            evt_tag_printf("block", "%.*s", (gint) result->len, result->str));
  return TRUE;
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
  gint context_value;

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
  context_value = cfg_lexer_lookup_context_type_by_name(context);
  if (context_value == 0)
    {
      msg_error("confgen: context value is unknown",
                evt_tag_str("context", context));
      return FALSE;
    }
  exec = cfg_args_get(args, "exec");
  if (!exec)
    {
      msg_error("confgen: exec argument expected");
      return FALSE;
    }
  cfg_lexer_register_generator_plugin(plugin_context,
                                      confgen_exec_new(context_value, name, exec));
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
