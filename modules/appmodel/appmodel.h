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
#ifndef APPMODEL_H_INCLUDED
#define APPMODEL_H_INCLUDED 1

#include "module-config.h"
#include "application.h"

AppModelContext *appmodel_get_context(GlobalConfig *cfg);
void appmodel_register_application(GlobalConfig *cfg, Application *application);
void appmodel_iter_applications(GlobalConfig *cfg,
                                void (*foreach)(Application *app, gpointer user_data),
                                gpointer user_data);

#endif
