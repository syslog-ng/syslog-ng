/*
 * Copyright (c) 2010-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2010-2013 Gergely Nagy <algernon@balabit.hu>
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

#ifndef AFMONGODB_H_INCLUDED
#define AFMONGODB_H_INCLUDED

#include "driver.h"
#include "value-pairs.h"

LogDriver *afmongodb_dd_new(void);

void afmongodb_dd_set_servers(LogDriver *d, GList *servers);
void afmongodb_dd_set_host(LogDriver *d, const gchar *host);
void afmongodb_dd_set_port(LogDriver *d, gint port);
void afmongodb_dd_set_database(LogDriver *d, const gchar *database);
void afmongodb_dd_set_collection(LogDriver *d, const gchar *collection);
void afmongodb_dd_set_value_pairs(LogDriver *d, ValuePairs *vp);
void afmongodb_dd_set_safe_mode(LogDriver *d, gboolean state);
void afmongodb_dd_set_path(LogDriver *d, const gchar *path);

gboolean afmongodb_dd_check_address(LogDriver *d, gboolean local);

#endif
