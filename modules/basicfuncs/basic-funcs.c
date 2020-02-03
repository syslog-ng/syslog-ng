/*
 * Copyright (c) 2010-2015 Balabit
 * Copyright (c) 2010-2015 Balázs Scheidler
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

#include "plugin.h"
#include "template/simple-function.h"
#include "filter/filter-expr.h"
#include "filter/filter-expr-parser.h"
#include "cfg.h"
#include "parse-number.h"
#include "str-format.h"
#include "plugin-types.h"
#include "scratch-buffers.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* helper functions available for all modules */

void
_append_args_with_separator(gint argc, GString *argv[], GString *result, gchar separator)
{
  gint i;

  for (i = 0; i < argc; i++)
    {
      g_string_append_len(result, argv[i]->str, argv[i]->len);
      if (i < argc - 1)
        g_string_append_c(result, separator);
    }
}

/* in order to avoid having to declare all construct functions, we
 * include them all here. If it causes compilation times to increase
 * drastically, we should probably make them into separate compilation
 * units. (Bazsi) */
#include "urlencode.c"
#include "numeric-funcs.c"
#include "str-funcs.c"
#include "cond-funcs.c"
#include "ip-funcs.c"
#include "misc-funcs.c"
#include "list-funcs.c"
#include "tf-template.c"
#include "context-funcs.c"
#include "fname-funcs.c"

static Plugin basicfuncs_plugins[] =
{
  /* cond-funcs */
  TEMPLATE_FUNCTION_PLUGIN(tf_grep, "grep"),
  TEMPLATE_FUNCTION_PLUGIN(tf_if, "if"),
  TEMPLATE_FUNCTION_PLUGIN(tf_or, "or"),

  /* context related funcs */
  TEMPLATE_FUNCTION_PLUGIN(tf_context_lookup, "context-lookup"),
  TEMPLATE_FUNCTION_PLUGIN(tf_context_length, "context-length"),
  TEMPLATE_FUNCTION_PLUGIN(tf_context_values, "context-values"),

  /* str-funcs */
  TEMPLATE_FUNCTION_PLUGIN(tf_echo, "echo"),
  TEMPLATE_FUNCTION_PLUGIN(tf_length, "length"),
  TEMPLATE_FUNCTION_PLUGIN(tf_substr, "substr"),
  TEMPLATE_FUNCTION_PLUGIN(tf_strip, "strip"),
  TEMPLATE_FUNCTION_PLUGIN(tf_sanitize, "sanitize"),
  TEMPLATE_FUNCTION_PLUGIN(tf_lowercase, "lowercase"),
  TEMPLATE_FUNCTION_PLUGIN(tf_uppercase, "uppercase"),
  TEMPLATE_FUNCTION_PLUGIN(tf_replace_delimiter, "replace-delimiter"),
  TEMPLATE_FUNCTION_PLUGIN(tf_string_padding, "padding"),
  TEMPLATE_FUNCTION_PLUGIN(tf_binary, "binary"),
  TEMPLATE_FUNCTION_PLUGIN(tf_implode, "implode"),
  TEMPLATE_FUNCTION_PLUGIN(tf_explode, "explode"),

  /* fname-funcs */
  TEMPLATE_FUNCTION_PLUGIN(tf_dirname, "dirname"),
  TEMPLATE_FUNCTION_PLUGIN(tf_basename, "basename"),

  /* list-funcs */
  TEMPLATE_FUNCTION_PLUGIN(tf_list_concat, "list-concat"),
  TEMPLATE_FUNCTION_PLUGIN(tf_list_head, "list-head"),
  TEMPLATE_FUNCTION_PLUGIN(tf_list_nth, "list-nth"),
  TEMPLATE_FUNCTION_PLUGIN(tf_list_tail, "list-tail"),
  TEMPLATE_FUNCTION_PLUGIN(tf_list_slice, "list-slice"),
  TEMPLATE_FUNCTION_PLUGIN(tf_list_count, "list-count"),
  TEMPLATE_FUNCTION_PLUGIN(tf_list_append, "list-append"),
  TEMPLATE_FUNCTION_PLUGIN(tf_list_search, "list-search"),

  /* numeric-funcs */
  TEMPLATE_FUNCTION_PLUGIN(tf_num_plus, "+"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_minus, "-"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_multi, "*"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_div, "/"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_mod, "%"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_sum, "sum"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_min, "min"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_max, "max"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_average, "average"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_round, "round"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_ceil, "ceil"),
  TEMPLATE_FUNCTION_PLUGIN(tf_num_floor, "floor"),

  /* ip-funcs */
  TEMPLATE_FUNCTION_PLUGIN(tf_ipv4_to_int, "ipv4-to-int"),
  TEMPLATE_FUNCTION_PLUGIN(tf_indent_multi_line, "indent-multi-line"),
  TEMPLATE_FUNCTION_PLUGIN(tf_dns_resolve_ip, "dns-resolve-ip"),

  /* misc funcs */
  TEMPLATE_FUNCTION_PLUGIN(tf_env, "env"),
  TEMPLATE_FUNCTION_PLUGIN(tf_template, "template"),
  TEMPLATE_FUNCTION_PLUGIN(tf_urlencode, "url-encode"),
  TEMPLATE_FUNCTION_PLUGIN(tf_urldecode, "url-decode"),
  TEMPLATE_FUNCTION_PLUGIN(tf_base64encode, "base64-encode")
};

gboolean
basicfuncs_module_init(PluginContext *context, CfgArgs *args)
{
  plugin_register(context, basicfuncs_plugins, G_N_ELEMENTS(basicfuncs_plugins));
  return TRUE;
}

const ModuleInfo module_info =
{
  .canonical_name = "basicfuncs",
  .version = SYSLOG_NG_VERSION,
  .description = "The basicfuncs module provides various template functions for syslog-ng.",
  .core_revision = SYSLOG_NG_SOURCE_REVISION,
  .plugins = basicfuncs_plugins,
  .plugins_len = G_N_ELEMENTS(basicfuncs_plugins),
};
