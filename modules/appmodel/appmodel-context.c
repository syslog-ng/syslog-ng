/*
 * Copyright (c) 2015 Balabit
 * Copyright (c) 2015 Balazs Scheidler <balazs.scheidler@balabit.com>
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
#include "appmodel.h"

#include <string.h>

void
appmodel_object_init_instance(AppModelObject *self, const gchar *type, const gchar *name, const gchar *instance)
{
  self->type = g_strdup(type);
  self->name = g_strdup(name);
  self->instance = g_strdup(instance);
}

void
appmodel_object_free(AppModelObject *self)
{
  if (self->free_fn)
    self->free_fn(self);

  g_free(self->instance);
  g_free(self->type);
  g_free(self->name);

  g_free(self);
}

struct _AppModelContext
{
  /* the context structure is registered into GlobalConfig, thus it must be
   * derived from ModuleConfig */
  ModuleConfig super;
  GHashTable *objects;
  GPtrArray *object_ptrs;
};

static gboolean
_object_equal(gconstpointer v1, gconstpointer v2)
{
  AppModelObject *r1 = (AppModelObject *) v1;
  AppModelObject *r2 = (AppModelObject *) v2;

  if (strcmp(r1->name, r2->name) != 0)
    return FALSE;

  if (strcmp(r1->instance, r2->instance) != 0)
    return FALSE;

  return TRUE;
}

static guint
_object_hash(gconstpointer v)
{
  AppModelObject *r = (AppModelObject *) v;

  return g_str_hash(r->name) + g_str_hash(r->instance);
}

void
appmodel_context_register_object(AppModelContext *self, AppModelObject *app)
{
  AppModelObject *orig_app;

  orig_app = g_hash_table_lookup(self->objects, app);
  if (!orig_app)
    {
      g_hash_table_insert(self->objects, app, app);
      g_ptr_array_add(self->object_ptrs, app);
    }
  else
    {
      g_hash_table_replace(self->objects, app, app);

      g_ptr_array_remove(self->object_ptrs, orig_app);
      g_ptr_array_add(self->object_ptrs, app);
    }
}

AppModelObject *
appmodel_context_lookup_object(AppModelContext *self, const gchar *type, const gchar *name, const gchar *instance)
{
  AppModelObject lookup_app = { 0 };

  lookup_app.type = (gchar *) type;
  lookup_app.name = (gchar *) name;
  lookup_app.instance = (gchar *) instance;
  return (AppModelObject *) g_hash_table_lookup(self->objects, &lookup_app);
}

void
appmodel_context_iter_objects(AppModelContext *self,
                              const gchar *type,
                              AppModelContextIterFunc foreach,
                              gpointer user_data)
{
  gint i;

  for (i = 0; i < self->object_ptrs->len; i++)
    {
      AppModelObject *app = g_ptr_array_index(self->object_ptrs, i);

      if (strcmp(app->type, type) != 0)
        continue;
      foreach(app, user_data);
    }
}

void
appmodel_context_free_method(ModuleConfig *s)
{
  AppModelContext *self = (AppModelContext *) s;

  g_hash_table_destroy(self->objects);
  g_ptr_array_free(self->object_ptrs, TRUE);
  module_config_free_method(s);
}

AppModelContext *
appmodel_context_new(void)
{
  AppModelContext *self = g_new0(AppModelContext, 1);

  self->super.free_fn = appmodel_context_free_method;
  self->objects = g_hash_table_new_full(_object_hash, _object_equal,
                                        NULL, (GDestroyNotify) appmodel_object_free);
  self->object_ptrs = g_ptr_array_new();
  return self;
}

void
appmodel_context_free(AppModelContext *self)
{
  return module_config_free(&self->super);
}
