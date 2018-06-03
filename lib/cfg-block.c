/*
 * Copyright (c) 2002-2017 Balabit
 * Copyright (c) 1998-2017 Balázs Scheidler
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
#include "cfg-block.h"
#include "cfg-lexer-subst.h"
#include "cfg.h"
#include "str-utils.h"

/*
 * This class encapsulates a configuration block that the user defined
 * via the configuration file. It behaves like a macro, e.g. when
 * referenced the content of the block is expanded.
 *
 * Each block is identified by its name and the context (source,
 * destination, etc.) where it is meant to be used.
 *
 * A block has a set of name-value pairs to allow expansion to be
 * parameterized. The set of allowed NV pairs is defined at block
 * definition time
 */
typedef struct _CfgBlock CfgBlock;
struct _CfgBlock
{
  CfgBlockGenerator super;
  gchar *filename;
  gint line, column;
  gchar *content;
  CfgArgs *arg_defs;
};

static const gchar *
cfg_block_format_name(CfgBlockGenerator *s, gchar *buf, gsize buf_len)
{
  CfgBlock *self = (CfgBlock *) s;

  g_snprintf(buf, buf_len, "block %s %s() at %s:%d",
             cfg_lexer_lookup_context_name_by_type(self->super.context),
             self->super.name,
             self->filename ? : "#buffer",
             self->line);
  return buf;
}

/* token block args */

static void
_validate_args_callback(gpointer k, gpointer v, gpointer user_data)
{
  CfgArgs *defs = ((gpointer *) user_data)[0];
  const gchar *reference = ((gpointer *) user_data)[1];
  gboolean *problem_found = ((gpointer *) user_data)[2];

  if ((!defs || cfg_args_get(defs, k) == NULL))
    {
      if (cfg_args_is_accepting_varargs(defs))
        {
          msg_verbose("Unknown argument, adding it to __VARARGS__",
                      evt_tag_str("argument", k),
                      evt_tag_str("value", v),
                      evt_tag_str("reference", reference));
        }
      else
        {
          msg_error("Unknown argument specified to block reference",
                    evt_tag_str("argument", k),
                    evt_tag_str("value", v),
                    evt_tag_str("reference", reference));
          *problem_found = TRUE;
        }
    }
}

static gboolean
_validate_args(CfgArgs *self, CfgArgs *defs, const gchar *reference)
{
  gboolean problem_found = FALSE;
  gpointer validate_params[] = { defs, (gchar *) reference, &problem_found };

  cfg_args_foreach(self, _validate_args_callback, validate_params);

  if (problem_found)
    return FALSE;

  return TRUE;
}


/*
 * cfg_block_generate:
 *
 * This is a CfgBlockGeneratorFunc, which takes a CfgBlock defined by
 * the user, substitutes backtick values and generates input tokens
 * for the lexer.
 */
gboolean
cfg_block_generate(CfgBlockGenerator *s, GlobalConfig *cfg, CfgArgs *args, GString *result, const gchar *reference)
{
  CfgBlock *self = (CfgBlock *) s;
  gchar *value;
  gsize length;
  GError *error = NULL;
  gchar buf[256];

  if (!_validate_args(args, self->arg_defs, reference))
    return FALSE;

  if (cfg_args_is_accepting_varargs(self->arg_defs))
    {
      gchar *formatted_varargs = cfg_args_format_varargs(args, self->arg_defs);
      cfg_args_set(args, "__VARARGS__", formatted_varargs);
      g_free(formatted_varargs);
    }

  value = cfg_lexer_subst_args_in_input(cfg->globals, self->arg_defs, args, self->content, -1, &length, &error);
  if (!value)
    {
      msg_warning("Syntax error while resolving backtick references in block",
                  evt_tag_str("block_definition", cfg_block_generator_format_name(s, buf, sizeof(buf))),
                  evt_tag_str("error", error->message),
                  NULL);
      g_clear_error(&error);
      return FALSE;
    }

  g_string_assign_len(result, value, length);
  g_free(value);
  return TRUE;
}

/*
 * Free a user defined block.
 */
void
cfg_block_free_instance(CfgBlockGenerator *s)
{
  CfgBlock *self = (CfgBlock *) s;

  g_free(self->filename);
  g_free(self->content);
  cfg_args_unref(self->arg_defs);
  cfg_block_generator_free_instance(s);
}


/*
 * Construct a user defined block.
 */
CfgBlockGenerator *
cfg_block_new(gint context, const gchar *name, const gchar *content, CfgArgs *arg_defs, YYLTYPE *lloc)
{
  CfgBlock *self = g_new0(CfgBlock, 1);

  cfg_block_generator_init_instance(&self->super, context, name);
  self->super.free_fn = cfg_block_free_instance;
  self->super.generate = cfg_block_generate;
  self->super.format_name = cfg_block_format_name;
  self->super.suppress_backticks = TRUE;
  self->content = g_strdup(content);
  self->filename = g_strdup(lloc->level->name);
  self->line = lloc->first_line;
  self->column = lloc->first_column;
  self->arg_defs = arg_defs;
  return &self->super;
}
