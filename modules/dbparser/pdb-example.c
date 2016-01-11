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
#include "pdb-example.h"

void
pdb_example_free(PDBExample *self)
{
  gint i;

  if (self->rule)
    pdb_rule_unref(self->rule);

  if (self->message)
    g_free(self->message);

  if (self->program)
    g_free(self->program);

  if (self->values)
    {
      for (i = 0; i < self->values->len; i++)
        {
          gchar **nv = g_ptr_array_index(self->values, i);

          g_free(nv[0]);
          g_free(nv[1]);
          g_free(nv);
        }

      g_ptr_array_free(self->values, TRUE);
    }

  g_free(self);
}
