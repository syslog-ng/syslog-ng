/*
 * Copyright (c) 2023 Balázs Scheidler <balazs.scheidler@axoflow.com>
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

#include "transformation.h"

void
transformation_set_translate(Transformation *self, const gchar *translate_expr)
{
  g_free(self->translate_expr);
  self->translate_expr = g_strdup(translate_expr);
}

static void
transformation_free(AppModelObject *s)
{
  Transformation *self = (Transformation *) s;
  g_free(self->translate_expr);
}

Transformation *
transformation_new(const gchar *name, const gchar *target)
{
  Transformation *self = g_new0(Transformation, 1);

  appmodel_object_init_instance(&self->super, TRANSFORMATION_TYPE_NAME, name, target);
  self->super.free_fn = transformation_free;
  return self;
}
