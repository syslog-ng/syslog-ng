/*
 * Copyright (c) 2017 Balabit
 * Copyright (c) 2017 Balazs Scheidler <balazs.scheidler@balabit.com>
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
#ifndef APPMODEL_CONTEXT_H_INCLUDED
#define APPMODEL_CONTEXT_H_INCLUDED 1

#include "module-config.h"

typedef struct _AppModelContext AppModelContext;
typedef struct _AppModelObject AppModelObject;
typedef void(*AppModelContextIterFunc)(AppModelObject *object, gpointer user_data);

struct _AppModelObject
{
  void (*free_fn)(AppModelObject *s);
  gchar *type;
  gchar *name;
  gchar *instance;
};

void appmodel_object_init_instance(AppModelObject *self,
                                   const gchar *type, const gchar *name,
                                   const gchar *instance);
void appmodel_object_free(AppModelObject *self);

void appmodel_context_iter_objects(AppModelContext *self,
                                   const gchar *type,
                                   AppModelContextIterFunc foreach,
                                   gpointer user_data);
AppModelObject *appmodel_context_lookup_object(AppModelContext *self,
                                               const gchar *type, const gchar *name,
                                               const gchar *instance);
void appmodel_context_register_object(AppModelContext *self, AppModelObject *object);
void appmodel_context_free(AppModelContext *self);
AppModelContext *appmodel_context_new(void);

#endif
