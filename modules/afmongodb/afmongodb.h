/*
 * Copyright (c) 2010-2016 Balabit
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

#include "syslog-ng.h"
#include "driver.h"
#include "template/templates.h"
#include "value-pairs/value-pairs.h"

LogDriver *afmongodb_dd_new(GlobalConfig *cfg);

void afmongodb_dd_set_uri(LogDriver *d, const gchar *uri);
void afmongodb_dd_set_collection(LogDriver *d, LogTemplate *collection_template);
void afmongodb_dd_set_value_pairs(LogDriver *d, ValuePairs *vp);

LogTemplateOptions *afmongodb_dd_get_template_options(LogDriver *s);

gboolean afmongodb_dd_private_uri_init(LogDriver *self);

#endif
