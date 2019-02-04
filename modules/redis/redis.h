/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2013 Tihamer Petrovics <tihameri@gmail.com>
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

#ifndef REDIS_H_INCLUDED
#define REDIS_H_INCLUDED

#include "driver.h"

LogDriver *redis_dd_new(GlobalConfig *cfg);

void redis_dd_set_host(LogDriver *d, const gchar *host);
void redis_dd_set_port(LogDriver *d, gint port);
void redis_dd_set_auth(LogDriver *d, const gchar *auth);
void redis_dd_set_command_ref(LogDriver *d, const gchar *command,
                              LogTemplate *key,
                              LogTemplate *param1, LogTemplate *param2);
LogTemplateOptions *redis_dd_get_template_options(LogDriver *d);

#endif
