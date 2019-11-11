/*
 * Copyright (c) 2019 Balabit
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

#include "python-persist.h"

static void
format_default_stats_name(gchar *buffer, gsize size, const gchar *module, const gchar *name)
{
  g_snprintf(buffer, size, "%s,%s", module, name);
}

const gchar *
python_format_stats_instance(LogPipe *p, const gchar *module, const gchar *class)
{
  static gchar persist_name[1024];

  if (p->persist_name)
    format_default_stats_name(persist_name, sizeof(persist_name), module, p->persist_name);
  else
    format_default_stats_name(persist_name, sizeof(persist_name), module, class);

  return persist_name;
}

static void
format_default_persist_name_with_persist_name(gchar *buffer, gsize size, const gchar *module, const gchar *persist_name)
{
  g_snprintf(buffer, size, "%s.%s", module, persist_name);
}

static void
format_default_persist_name_with_class(gchar *buffer, gsize size, const gchar *module, const gchar *class)
{
  g_snprintf(buffer, size, "%s(%s)", module, class);
}

const gchar *
python_format_persist_name(const LogPipe *p, const gchar *module, const gchar *class)
{
  static gchar persist_name[1024];

  if (p->persist_name)
    format_default_persist_name_with_persist_name(persist_name, sizeof(persist_name), module, p->persist_name);
  else
    format_default_persist_name_with_class(persist_name, sizeof(persist_name), module, class);

  return persist_name;
}
