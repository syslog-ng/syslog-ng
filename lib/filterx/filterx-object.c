/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#include "filterx-object.h"

void
filterx_type_init(FilterXType *type)
{
  FilterXType *parent = type->super_type;

  for (gint i = 0; i < G_N_ELEMENTS(type->__methods); i++)
    {
      if (!type->__methods[i])
        type->__methods[i] = parent->__methods[i];
    }
}

#define FILTERX_OBJECT_MAGIC_BIAS G_MAXINT32

void
filterx_object_free_method(FilterXObject *self)
{
  /* empty */
}

void
filterx_object_init_instance(FilterXObject *self, FilterXType *type)
{
  self->ref_cnt = 1;
  self->type = type;
}

FilterXObject *
filterx_object_new(FilterXType *type)
{
  FilterXObject *self = g_new0(FilterXObject, 1);
  filterx_object_init_instance(self, type);
  return self;
}

gboolean
filterx_object_freeze(FilterXObject *self)
{
  if (self->ref_cnt == FILTERX_OBJECT_MAGIC_BIAS)
    return FALSE;
  g_assert(self->ref_cnt == 1);
  self->ref_cnt = FILTERX_OBJECT_MAGIC_BIAS;
  return TRUE;
}

void
filterx_object_unfreeze_and_free(FilterXObject *self)
{
  g_assert(self->ref_cnt == FILTERX_OBJECT_MAGIC_BIAS);
  self->ref_cnt = 1;
  filterx_object_unref(self);
}

FilterXObject *
filterx_object_ref(FilterXObject *self)
{
  if (!self)
    return NULL;

  if (self->ref_cnt == FILTERX_OBJECT_MAGIC_BIAS)
    return self;
  self->ref_cnt++;
  return self;
}

void
filterx_object_unref(FilterXObject *self)
{
  if (!self)
    return;

  if (self->ref_cnt == FILTERX_OBJECT_MAGIC_BIAS)
    return;

  g_assert(self->ref_cnt > 0);
  if (--self->ref_cnt == 0)
    {
      self->type->free_fn(self);
      g_free(self);
    }
}

FilterXType FILTERX_TYPE_NAME(object) =
{
  .super_type = NULL,
  .name = "object",
  .free_fn = filterx_object_free_method,
};
