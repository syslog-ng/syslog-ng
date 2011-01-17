/*
 * Copyright (c) 2002-2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2010-2011 Gergely Nagy <algernon@balabit.hu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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

#ifndef AFMONGODB_H_INCLUDED
#define AFMONGODB_H_INCLUDED

#include "driver.h"

LogDriver *afmongodb_dd_new(void);

void afmongodb_dd_set_host(LogDriver *d, const gchar *host);
void afmongodb_dd_set_port(LogDriver *d, gint port);
void afmongodb_dd_set_database(LogDriver *d, const gchar *database);
void afmongodb_dd_set_collection(LogDriver *d, const gchar *collection);
void afmongodb_dd_set_values(LogDriver *d, GList *values);
void afmongodb_dd_set_keys(LogDriver *d, GList *keys);
void afmongodb_dd_set_user(LogDriver *d, const gchar *user);
void afmongodb_dd_set_password(LogDriver *d, const gchar *password);
void afmongodb_dd_set_log_fifo_size(LogDriver *d, guint32 size);

#endif
