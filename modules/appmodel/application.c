/*
 * Copyright (c) 2002-2017 Balabit
 * Copyright (c) 1998-2017 Balázs Scheidler
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

#include "application.h"

void
application_set_filter(Application *self, const gchar *filter_expr)
{
  g_free(self->filter_expr);
  self->filter_expr = g_strdup(filter_expr);
}

void
application_set_parser(Application *self, const gchar *parser_expr)
{
  g_free(self->parser_expr);
  self->parser_expr = g_strdup(parser_expr);
}

Application *
application_new(const gchar *name, const gchar *topic)
{
  Application *self = g_new0(Application, 1);

  self->name = g_strdup(name);
  self->topic = g_strdup(topic);
  return self;
}

void
application_free(Application *self)
{
  g_free(self->name);
  g_free(self->topic);
  g_free(self->filter_expr);
  g_free(self->parser_expr);
  g_free(self);
}
