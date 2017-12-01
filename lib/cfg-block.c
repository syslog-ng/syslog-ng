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
  gchar *content;
  CfgArgs *arg_defs;
};

/*
 * cfg_block_generate:
 *
 * This is a CfgBlockGeneratorFunc, which takes a CfgBlock defined by
 * the user, substitutes backtick values and generates input tokens
 * for the lexer.
 */
gboolean
cfg_block_generate(CfgBlockGenerator *s, GlobalConfig *cfg, CfgArgs *args, GString *result)
{
  CfgBlock *self = (CfgBlock *) s;
  gchar *value;
  gsize length;
  GError *error = NULL;

  cfg_args_set(args, "__VARARGS__", cfg_args_format_varargs(args, self->arg_defs));

  value = cfg_lexer_subst_args_in_input(cfg->globals, self->arg_defs, args, self->content, -1, &length, &error);
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
  self->super.suppress_backticks = TRUE;
  self->content = g_strdup(content);
  self->arg_defs = arg_defs;
  return &self->super;
}
