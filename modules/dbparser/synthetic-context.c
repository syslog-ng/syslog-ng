/*
 * Copyright (c) 2016 Balabit
 * Copyright (c) 2016 Balázs Scheidler
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

#include "synthetic-context.h"
#include "pdb-error.h"

#include <string.h>

void
synthetic_context_set_context_id_template(SyntheticContext *self, LogTemplate *context_id_template)
{
  if (self->id_template)
    log_template_unref(self->id_template);
  self->id_template = context_id_template;
}

void
synthetic_context_set_context_timeout(SyntheticContext *self, gint timeout)
{
  self->timeout = timeout;
}

void
synthetic_context_set_context_scope(SyntheticContext *self, const gchar *scope, GError **error)
{
  int context_scope = correlation_key_lookup_scope(scope);
  if (context_scope < 0)
    {
      self->scope = RCS_GLOBAL;
      g_set_error(error, PDB_ERROR, PDB_ERROR_FAILED, "Unknown context scope: %s", scope);
    }
  else
    self->scope = context_scope;
}

void
synthetic_context_init(SyntheticContext *self)
{
  memset(self, 0, sizeof(*self));
  self->scope = RCS_PROCESS;
}

void
synthetic_context_deinit(SyntheticContext *self)
{
  if (self->id_template)
    log_template_unref(self->id_template);
}
