/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Viktor Juhász <viktor.juhasz@balabit.com>
 * Copyright (c) 2014 Viktor Tusa <viktor.tusa@balabit.com>
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

#include "config_parse_lib.h"
#include "cfg.h"
#include "cfg-parser.h"
#include "plugin.h"

static gchar **
split_plugin_name_and_config(const gchar *config_to_parse)
{
  gchar **delimited = NULL;

  if (!config_to_parse || strlen(config_to_parse) == 0)
    return NULL;

  delimited = g_strsplit(config_to_parse, "(", 2);
  if (!delimited)
    return NULL;

  if (g_strv_length(delimited) == 2)
    {
      gchar *tmp = delimited[1];
      delimited[1] = g_strdup_printf("(%s", tmp);
      g_free(tmp);
      return delimited;
    }

  g_strfreev(delimited);
  return NULL;
}

static gpointer
parse_plugin_config(const gchar *config_to_parse, gint context, gpointer arg)
{
  gpointer result = NULL;
  CfgLexer *old_lexer = configuration->lexer;
  CfgLexer *lexer = NULL;
  YYLTYPE *yylloc = NULL;
  Plugin *plugin = NULL;
  gchar **delimited;

  delimited = split_plugin_name_and_config(config_to_parse);
  if (!delimited)
    {
      fprintf(stderr, "Error parsing expression\n");
      return NULL;
    }

  lexer = cfg_lexer_new_buffer(configuration, delimited[1], strlen(delimited[1]));
  if (!lexer)
    {
      fprintf(stderr, "Error parsing expression\n");
      goto finish;
    }

  yylloc = g_new0(YYLTYPE, 1);
  yylloc->first_column = 1;
  yylloc->first_line = 1;
  yylloc->last_column = 1;
  yylloc->last_line = 1;
  yylloc->level = &lexer->include_stack[0];

  plugin = cfg_find_plugin(configuration, context, delimited[0]);
  if (!plugin)
    {
      fprintf(stderr, "Error parsing expression\n");
      goto finish;
    }

  configuration->lexer = lexer;
  cfg_set_global_paths(configuration);

  cfg_lexer_push_context(lexer, main_parser.context, main_parser.keywords, main_parser.name);
  result = cfg_parse_plugin(configuration, plugin, yylloc, arg);
  cfg_lexer_pop_context(lexer);

  if (!result)
    {
      fprintf(stderr, "Error parsing expression\n");
      goto finish;
    }

finish:
  configuration->lexer = old_lexer;
  if (lexer)
    cfg_lexer_free(lexer);
  g_free(yylloc);
  g_strfreev(delimited);
  return result;
}

static gboolean
parse_general_config(const gchar *config_to_parse, gint context, gpointer arg)
{
  gpointer result = NULL;
  CfgLexer *lexer = cfg_lexer_new_buffer(configuration, config_to_parse, strlen(config_to_parse));

  if (!cfg_run_parser(configuration, lexer, &main_parser, &result, arg))
    {
      fprintf(stderr, "Error parsing expression\n");
      return FALSE;
    }

  return TRUE;
}

gboolean
parse_config(const gchar *config_to_parse, gint context, gpointer arg, gpointer *result)
{
  if (!config_to_parse || strlen(config_to_parse) == 0)
    return FALSE;

  if (result)
    {
      *result = parse_plugin_config(config_to_parse, context, arg);
      if (*result)
        return TRUE;
    }

  if (main_parser.context != context)
    return FALSE;

  return parse_general_config(config_to_parse, context, arg);
}
