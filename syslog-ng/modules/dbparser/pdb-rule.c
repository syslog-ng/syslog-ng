/*
 * Copyright (c) 2002-2013, 2015 Balabit
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
#include "pdb-rule.h"
#include "pdb-error.h"

void
pdb_rule_set_class(PDBRule *self, const gchar *class)
{
  gchar class_tag_text[32];

  if (self->class)
    {
      g_free(self->class);
    }
  else
    {
      g_snprintf(class_tag_text, sizeof(class_tag_text), ".classifier.%s", class);
      synthetic_message_add_tag(&self->msg, class_tag_text);
    }
  self->class = class ? g_strdup(class) : NULL;

}

void
pdb_rule_set_rule_id(PDBRule *self, const gchar *rule_id)
{
  if (self->rule_id)
    g_free(self->rule_id);
  self->rule_id = g_strdup(rule_id);
}

void
pdb_rule_add_action(PDBRule *self, PDBAction *action)
{
  if (!self->actions)
    self->actions = g_ptr_array_new();
  g_ptr_array_add(self->actions, action);
}

gchar *
pdb_rule_get_name(PDBRule *self)
{
  if (self)
    return self->rule_id;
  else
    return NULL;
}

PDBRule *
pdb_rule_new(void)
{
  PDBRule *self = g_new0(PDBRule, 1);

  g_atomic_counter_set(&self->ref_cnt, 1);
  synthetic_context_init(&self->context);
  synthetic_message_init(&self->msg);
  return self;
}

PDBRule *
pdb_rule_ref(PDBRule *self)
{
  g_atomic_counter_inc(&self->ref_cnt);
  return self;
}

void
pdb_rule_unref(PDBRule *self)
{
  if (g_atomic_counter_dec_and_test(&self->ref_cnt))
    {

      if (self->actions)
        {
          g_ptr_array_foreach(self->actions, (GFunc) pdb_action_free, NULL);
          g_ptr_array_free(self->actions, TRUE);
        }

      if (self->rule_id)
        g_free(self->rule_id);

      if (self->class)
        g_free(self->class);

      synthetic_context_deinit(&self->context);
      synthetic_message_deinit(&self->msg);
      g_free(self);
    }
}
