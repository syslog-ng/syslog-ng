/*
 * Copyright (c) 2002-2013, 2015 BalaBit
 * Copyright (c) 1998-2013, 2015 Balázs Scheidler
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
#include "pdb-action.h"
#include "filter/filter-expr-parser.h"
#include "misc.h"
#include <stdlib.h>

void
pdb_action_set_condition(PDBAction *self, GlobalConfig *cfg, const gchar *filter_string, GError **error)
{
  CfgLexer *lexer;

  lexer = cfg_lexer_new_buffer(filter_string, strlen(filter_string));
  if (!cfg_run_parser(cfg, lexer, &filter_expr_parser, (gpointer *) &self->condition, NULL))
    {
      g_set_error(error, 0, 1, "Error compiling conditional expression");
      self->condition = NULL;
      return;
    }
}

void
pdb_action_set_rate(PDBAction *self, const gchar *rate_)
{
  gchar *slash;
  gchar *rate;

  rate = g_strdup(rate_);

  slash = strchr(rate, '/');
  if (!slash)
    {
      self->rate = atoi(rate);
      self->rate_quantum = 1;
    }
  else
    {
      *slash = 0;
      self->rate = atoi(rate);
      self->rate_quantum = atoi(slash + 1);
      *slash = '/';
    }
  if (self->rate_quantum == 0)
    self->rate_quantum = 1;
  g_free(rate);
}

void
pdb_action_set_trigger(PDBAction *self, const gchar *trigger, GError **error)
{
  if (strcmp(trigger, "match") == 0)
    self->trigger = RAT_MATCH;
  else if (strcmp(trigger, "timeout") == 0)
    self->trigger = RAT_TIMEOUT;
  else
    g_set_error(error, 0, 1, "Unknown trigger type: %s", trigger);
}

void
pdb_action_set_message_inheritance(PDBAction *self, const gchar *inherit_properties, GError **error)
{
  if (strcasecmp(inherit_properties, "context") == 0)
    self->content.inherit_mode = RAC_MSG_INHERIT_CONTEXT;
  else if (inherit_properties[0] == 'T' || inherit_properties[0] == 't' ||
      inherit_properties[0] == '1')
    self->content.inherit_mode = RAC_MSG_INHERIT_LAST_MESSAGE;
  else if (inherit_properties[0] == 'F' || inherit_properties[0] == 'f' ||
           inherit_properties[0] == '0')
    self->content.inherit_mode = RAC_MSG_INHERIT_NONE;
  else
    g_set_error(error, 0, 1, "Unknown inheritance type: %s", inherit_properties);
}


PDBAction *
pdb_action_new(gint id)
{
  PDBAction *self;

  self = g_new0(PDBAction, 1);

  self->trigger = RAT_MATCH;
  self->content_type = RAC_NONE;
  self->id = id;
  return self;
}

void
pdb_action_free(PDBAction *self)
{
  if (self->condition)
    filter_expr_unref(self->condition);
  if (self->content_type == RAC_MESSAGE)
    synthetic_message_deinit(&self->content.message);
  g_free(self);
}
