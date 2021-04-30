/*
 * Copyright (c) 2012 Nagy, Attila <bra@fsn.hu>
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2013 Viktor Tusa <tusa@balabit.hu>
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

#ifndef AFSTOMP_H_INCLUDED
#define AFSTOMP_H_INCLUDED

#include "driver.h"
#include "value-pairs/value-pairs.h"

LogDriver *afstomp_dd_new(GlobalConfig *cfg);

void afstomp_dd_set_host(LogDriver *d, const gchar *host);
void afstomp_dd_set_port(LogDriver *d, gint port);
void afstomp_dd_set_destination(LogDriver *d, const gchar *destination);
void afstomp_dd_set_body(LogDriver *d, LogTemplate *body_template);
void afstomp_dd_set_persistent(LogDriver *d, gboolean persistent);
void afstomp_dd_set_ack(LogDriver *d, gboolean ack);
void afstomp_dd_set_user(LogDriver *d, const gchar *user);
void afstomp_dd_set_password(LogDriver *d, const gchar *password);
void afstomp_dd_set_value_pairs(LogDriver *d, ValuePairs *vp);

LogTemplateOptions *afstomp_dd_get_template_options(LogDriver *s);


#endif
