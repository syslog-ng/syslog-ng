/*
 * Copyright (c) 2010-2016 Balabit
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

#ifndef AFMONGODB_LEGACY_GRAMMAR_H_
#define AFMONGODB_LEGACY_GRAMMAR_H_

#include "syslog-ng.h"
#include "driver.h"

gboolean afmongodb_dd_validate_socket_combination(LogDriver *d);
gboolean afmongodb_dd_validate_network_combination(LogDriver *d);
void afmongodb_dd_set_servers(LogDriver *d, GList *servers);
void afmongodb_dd_set_host(LogDriver *d, const gchar *host);
void afmongodb_dd_set_port(LogDriver *d, gint port);
void afmongodb_dd_set_database(LogDriver *d, const gchar *database);
void afmongodb_dd_set_user(LogDriver *d, const gchar *user);
void afmongodb_dd_set_password(LogDriver *d, const gchar *password);
void afmongodb_dd_set_safe_mode(LogDriver *d, gboolean state);
void afmongodb_dd_set_path(LogDriver *d, const gchar *path);

#endif
