/*
 * Copyright (c) 2016 Balabit
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

#ifndef ADD_CONTEXTUAL_DATA_H_INCLUDED
#define ADD_CONTEXTUAL_DATA_H_INCLUDED

#include "parser/parser-expr.h"
#include "syslog-ng.h"
#include "template/common-template-typedefs.h"
#include "add-contextual-data-selector.h"

LogParser *add_contextual_data_parser_new(GlobalConfig *cfg);

LogTemplateOptions *add_contextual_data_get_template_options(LogParser *d);

void add_contextual_data_set_database_key_template(LogParser *p,
                                                   const gchar *key);

void add_contextual_data_set_filename(LogParser *p, const gchar *filename);

void add_contextual_data_set_database_selector_template(LogParser *p,
                                                        const gchar *
                                                        selector);

void add_contextual_data_set_database_default_selector(LogParser *p,
                                                       const gchar *
                                                       default_selector);

void add_contextual_data_set_prefix(LogParser *p, const gchar *perfix);
void add_contextual_data_set_filters_path(LogParser *p, const gchar *filename);

void add_contextual_data_set_selector(LogParser *p, AddContextualDataSelector *selector);

void add_contextual_data_set_selector_filter(LogParser *p, const gchar *filename);

#endif
