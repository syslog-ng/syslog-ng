/*
 * Copyright (c) 2019 Balabit
 * Copyright (c) 2019 Kokan <kokaipeter@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "filter/optimizer/filter-expr-optimizer-registry.h"

#include "filter/optimizer/concatenate-or-filters.h"
#include "filter/optimizer/filter-tree-printer.h"

GList *filter_optimizer_get_optimizers(void)
{
  GList *optimizers = NULL;

  optimizers = g_list_append(optimizers, concatenate_or_filters_get_instance());
  optimizers = g_list_append(optimizers, filter_tree_printer_get_instance());

  return optimizers;
}

