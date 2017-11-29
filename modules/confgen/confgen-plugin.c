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
#include "cfg-lexer.h"
#include "messages.h"
#include "plugin.h"
#include "plugin-types.h"

#include <string.h>
#include <errno.h>

gboolean
confgen_generate(CfgLexer *lexer, gint type, const gchar *name, CfgArgs *args, gpointer user_data)
{
  gchar *value;
  gsize value_len = 0;
  FILE *out;
  gchar *exec = (gchar *) user_data;
  gsize res;
  gchar buf[256];
  gboolean result;

  g_snprintf(buf, sizeof(buf), "%s confgen %s", cfg_lexer_lookup_context_name_by_type(type), name);
  if (!cfg_args_validate(args, NULL, buf))
    {
      msg_error("confgen: confgen invocations do not process arguments, but your argument list is not empty",
                evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(type)),
                evt_tag_str("block", name));
      return FALSE;
    }

  out = popen((gchar *) user_data, "r");
  if (!out)
    {
      msg_error("confgen: Error executing generator program",
                evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(type)),
                evt_tag_str("block", name),
                evt_tag_str("exec", exec),
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
                evt_tag_str("block", name),
                evt_tag_str("exec", exec),
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

gboolean
confgen_module_init(GlobalConfig *cfg, CfgArgs *args)
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
  cfg_lexer_register_block_generator(cfg->lexer, cfg_lexer_lookup_context_type_by_name(context), name, confgen_generate,
                                     g_strdup(exec), g_free);
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
