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

/* TransformationStep: a named filterx expression */

TransformationStep *
transformation_step_new(const gchar *name, const gchar *expr)
{
  TransformationStep *self = g_new0(TransformationStep, 1);
  self->name = g_strdup(name);
  self->expr = g_strdup(expr);
  return self;
}

void
transformation_step_free(TransformationStep *self)
{
  g_free(self->name);
  g_free(self->expr);
}

/* TransformationBlock: named list of steps */

void
transformation_block_add_step(TransformationBlock *self, const gchar *name, const gchar *expr)
{
  self->steps = g_list_append(self->steps, transformation_step_new(name, expr));
}

TransformationBlock *
transformation_block_new(const gchar *name)
{
  TransformationBlock *self = g_new0(TransformationBlock, 1);
  self->name = g_strdup(name);
  return self;
}

void
transformation_block_free(TransformationBlock *self)
{
  g_free(self->name);
  g_list_free_full(self->steps, (GDestroyNotify) transformation_step_free);
  g_free(self);
}

/* Transformation */
/* list of blocks */

void
transformation_add_block(Transformation *self, TransformationBlock *block)
{
  self->blocks = g_list_append(self->blocks, block);
}

static void
transformation_free(AppModelObject *s)
{
  Transformation *self = (Transformation *) s;

  g_list_free_full(self->blocks, (GDestroyNotify) transformation_block_free);
}

Transformation *
transformation_new(const gchar *app, const gchar *flavour)
{
  Transformation *self = g_new0(Transformation, 1);

  appmodel_object_init_instance(&self->super, TRANSFORMATION_TYPE_NAME, app, flavour);
  self->super.free_fn = transformation_free;
  return self;
}
