/*
 * Copyright (c) 2002-2015 Balabit
 * Copyright (c) 1998-2015 Balázs Scheidler
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

#ifndef KVTAGGER_H_INCLUDED
#define KVTAGGER_H_INCLUDED

#include "parser/parser-expr.h"
#include "syslog-ng.h"
#include "template/common-template-typedefs.h"

LogParser *kvtagger_parser_new(GlobalConfig *cfg);

LogTemplateOptions * kvtagger_get_template_options(LogParser *d);
void kvtagger_set_database_key_template(LogParser *p, const gchar *key);
void kvtagger_set_filename(LogParser *p, const gchar * filename);
void kvtagger_set_database_selector_template(LogParser *p, const gchar *selector);
void kvtagger_set_database_default_selector(LogParser *p, const gchar *default_selector);

#endif
