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
#include "cfg-lexer.h"
#include "cfg-lexer-subst.h"

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
  gchar *content;
  CfgArgs *arg_defs;
};

static void
_resolve_unknown_blockargs_as_varargs(gpointer key, gpointer value, gpointer user_data)
{
  CfgArgs *defs = ((gpointer *) user_data)[0];
  GString *varargs = ((gpointer *) user_data)[1];

  if (cfg_args_get(defs, key) == NULL)
    {
      g_string_append_printf(varargs, "%s(%s) ", (gchar *)key, (gchar *)value);
    }
}

static void
_fill_varargs(CfgBlock *block, CfgArgs *args)
{
  GString *varargs = g_string_new("");
  gpointer user_data[] = { block->arg_defs, varargs };

  cfg_args_foreach(args, _resolve_unknown_blockargs_as_varargs, user_data);
  cfg_args_set(args, "__VARARGS__", varargs->str);
  g_string_free(varargs, TRUE);
}

/*
 * cfg_block_generate:
 *
 * This is a CfgBlockGeneratorFunc, which takes a CfgBlock defined by
 * the user, substitutes backtick values and generates input tokens
 * for the lexer.
 */
gboolean
cfg_block_generate(CfgBlockGenerator *s, GlobalConfig *cfg, CfgLexer *lexer, CfgArgs *args)
{
  CfgBlock *self = (CfgBlock *) s;
  gchar *value;
  gchar buf[256];
  gsize length;
  GError *error = NULL;
  gboolean result;

  g_snprintf(buf, sizeof(buf), "%s block %s", cfg_lexer_lookup_context_name_by_type(self->super.context),
             self->super.name);
  _fill_varargs(self, args);

  value = cfg_lexer_subst_args_in_input(lexer->globals, self->arg_defs, args, self->content, -1, &length, &error);

  if (!value)
    {
      msg_warning("Syntax error while resolving backtick references in block",
                  evt_tag_str("context", cfg_lexer_lookup_context_name_by_type(self->super.context)),
                  evt_tag_str("block", self->super.name),
                  evt_tag_str("error", error->message),
                  NULL);
      g_clear_error(&error);
      return FALSE;
    }

  result = cfg_lexer_include_buffer_without_backtick_substitution(lexer, buf, value, length);
  g_free(value);
  return result;
}

/*
 * Free a user defined block.
 */
void
cfg_block_free_instance(CfgBlockGenerator *s)
{
  CfgBlock *self = (CfgBlock *) s;

  g_free(self->content);
  cfg_args_unref(self->arg_defs);
  cfg_block_generator_free_instance(s);
}


/*
 * Construct a user defined block.
 */
CfgBlockGenerator *
cfg_block_new(gint context, const gchar *name, const gchar *content, CfgArgs *arg_defs)
{
  CfgBlock *self = g_new0(CfgBlock, 1);

  cfg_block_generator_init_instance(&self->super, context, name);
  self->super.free_fn = cfg_block_free_instance;
  self->super.generate = cfg_block_generate;
  self->content = g_strdup(content);
  self->arg_defs = arg_defs;
  return &self->super;
}
