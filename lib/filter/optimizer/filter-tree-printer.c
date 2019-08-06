/*
 * Copyright (c) 2019 Balabit
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
#include "filter/optimizer/filter-tree-printer.h"

static gpointer
_filter_tree_printer_init(FilterExprNode *root)
{
  gint *cookie = g_malloc0(sizeof(gint)); // indentation
  *cookie = 20;
  msg_trace("filter-optimizer: ", evt_tag_printf("tree", "%-*s%s\n", *cookie, "parent", "child(s)"));
  return cookie;
}

static void
_filter_tree_printer_deinit(gpointer cookie)
{
  g_free(cookie);
}

static void
_filter_tree_printer_cb(FilterExprNode *current, FilterExprNode *parent, GPtrArray *childs, gpointer cookie)
{
  GString *line = g_string_new("");
  if (parent == NULL)
    g_string_append_printf(line, "%-*s", *(gint *)cookie, "root");
  else
    g_string_append_printf(line, "%-*s", *(gint *)cookie, parent->type);

  if (childs)
    {
      gint i;
      for (i = 0; i < childs->len; i++)
        {
          FilterExprNode *child = (FilterExprNode *)g_ptr_array_index(childs, i);
          g_string_append_printf(line, "%s ", child->type);
        }
    }
  else
    {
      g_string_append(line, "leaf");
    }

  msg_trace("filter-optimizer: ", evt_tag_printf("tree", "%s", line->str));
  g_string_free(line, TRUE);
}

FilterExprOptimizer filter_tree_printer =
{
  .name = "filter-tree-printer",
  .init = _filter_tree_printer_init,
  .deinit = _filter_tree_printer_deinit,
  .cb = _filter_tree_printer_cb
};

FilterExprOptimizer *
filter_tree_printer_get_instance(void)
{
  return &filter_tree_printer;
}
