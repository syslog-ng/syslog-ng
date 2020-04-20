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

struct _AppModelContext
{
  /* the context structure is registered into GlobalConfig, thus it must be
   * derived from ModuleConfig */
  ModuleConfig super;
  GHashTable *applications;
  GPtrArray *application_ptrs;
};

static gboolean
_application_equal(gconstpointer v1, gconstpointer v2)
{
  Application *r1 = (Application *) v1;
  Application *r2 = (Application *) v2;

  if (strcmp(r1->name, r2->name) != 0)
    return FALSE;

  if (strcmp(r1->topic, r2->topic) != 0)
    return FALSE;

  return TRUE;
}

static guint
_application_hash(gconstpointer v)
{
  Application *r = (Application *) v;

  return g_str_hash(r->name) + g_str_hash(r->topic);
}

void
appmodel_context_register_application(AppModelContext *self, Application *app)
{
  Application *orig_app;

  orig_app = g_hash_table_lookup(self->applications, app);
  if (!orig_app)
    {
      g_hash_table_insert(self->applications, app, app);
      g_ptr_array_add(self->application_ptrs, app);
    }
  else
    {
      g_hash_table_replace(self->applications, app, app);

      g_ptr_array_remove(self->application_ptrs, orig_app);
      g_ptr_array_add(self->application_ptrs, app);
    }
}

Application *
appmodel_context_lookup_application(AppModelContext *self, const gchar *name, const gchar *topic)
{
  Application lookup_app = { 0 };

  lookup_app.name = (gchar *) name;
  lookup_app.topic = (gchar *) topic;
  return (Application *) g_hash_table_lookup(self->applications, &lookup_app);
}

void
appmodel_context_iter_applications(AppModelContext *self, void (*foreach)(Application *app, Application *base_app,
                                   gpointer user_data), gpointer user_data)
{
  gint i;

  for (i = 0; i < self->application_ptrs->len; i++)
    {
      Application *app = g_ptr_array_index(self->application_ptrs, i);

      if (strcmp(app->topic, "*") == 0)
        continue;

      Application *base_app = appmodel_context_lookup_application(self, app->name, "*");
      foreach(app, base_app, user_data);
    }
}

void
appmodel_context_free_method(ModuleConfig *s)
{
  AppModelContext *self = (AppModelContext *) s;

  g_hash_table_destroy(self->applications);
  g_ptr_array_free(self->application_ptrs, TRUE);
  module_config_free_method(s);
}

AppModelContext *
appmodel_context_new(void)
{
  AppModelContext *self = g_new0(AppModelContext, 1);

  self->super.free_fn = appmodel_context_free_method;
  self->applications = g_hash_table_new_full(_application_hash, _application_equal,
                                             NULL, (GDestroyNotify) application_free);
  self->application_ptrs = g_ptr_array_new();
  return self;
}

void
appmodel_context_free(AppModelContext *self)
{
  return module_config_free(&self->super);
}
