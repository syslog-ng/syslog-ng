/*
 * Copyright (c) 2002-2017 Balabit
 * Copyright (c) 1998-2017 Balázs Scheidler
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
#include "pdb-context.h"

static void
pdb_context_free(CorrelationContext *s)
{
  PDBContext *self = (PDBContext *) s;

  if (self->rule)
    pdb_rule_unref(self->rule);
  correlation_context_free_method(s);
}

PDBContext *
pdb_context_new(CorrelationKey *key)
{
  PDBContext *self = g_new0(PDBContext, 1);

  correlation_context_init(&self->super, key);
  self->super.free_fn = pdb_context_free;
  return self;
}
