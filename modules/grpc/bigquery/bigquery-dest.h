/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Attila Szakacs <attila.szakacs@axoflow.com>
 * Copyright (c) 2023 László Várady
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

#ifndef BIGQUERY_DEST_H
#define BIGQUERY_DEST_H

#include "syslog-ng.h"

#include "compat/cpp-start.h"
#include "driver.h"
#include "template/templates.h"

LogDriver *bigquery_dd_new(GlobalConfig *cfg);

void bigquery_dd_set_project(LogDriver *d, const gchar *project);
void bigquery_dd_set_dataset(LogDriver *d, const gchar *dataset);
void bigquery_dd_set_table(LogDriver *d, const gchar *table);

gboolean bigquery_dd_add_field(LogDriver *d, const gchar *name, const gchar *type, LogTemplate *value);
void bigquery_dd_set_protobuf_schema(LogDriver *d, const gchar *proto_path, GList *values);

LogTemplateOptions *bigquery_dd_get_template_options(LogDriver *d);

#include "compat/cpp-end.h"

#endif
