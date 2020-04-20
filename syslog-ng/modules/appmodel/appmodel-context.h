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
#include "application.h"

typedef struct _AppModelContext AppModelContext;

void appmodel_context_iter_applications(AppModelContext *self,
                                        void (*foreach)(Application *app, Application *base_app, gpointer user_data),
                                        gpointer user_data);
Application *appmodel_context_lookup_application(AppModelContext *self, const gchar *name, const gchar *topic);
void appmodel_context_register_application(AppModelContext *self, Application *app);
void appmodel_context_free(AppModelContext *self);
AppModelContext *appmodel_context_new(void);

#endif
