/*
 * Copyright (c) 2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2013 Gergely Nagy <algernon@balabit.hu>
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

#include "filter-in-list.h"
#include "logmsg.h"
#include "misc.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct _FilterInList
{
  FilterExprNode super;
  NVHandle value_handle;
  GTree *tree;
} FilterInList;

static gboolean
filter_in_list_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterInList *self = (FilterInList *)s;
  LogMessage *msg = msgs[0];
  const gchar *value;
  gssize len = 0;

  value = log_msg_get_value(msg, self->value_handle, &len);
  APPEND_ZERO(value, value, len);

  return (g_tree_lookup(self->tree, value) != NULL) ^ s->comp;
}

static void
filter_in_list_free(FilterExprNode *s)
{
  FilterInList *self = (FilterInList *)s;

  g_tree_destroy(self->tree);
}

FilterExprNode *
filter_in_list_new(const gchar *list_file, const gchar *property)
{
  FilterInList *self;
  FILE *stream;
  size_t n;
  gchar *line = NULL;

  stream = fopen(list_file, "r");
  if (!stream)
    {
      msg_error("Error opening in-list filter list file",
                evt_tag_str("file", list_file),
                evt_tag_errno("errno", errno),
                NULL);
      return NULL;
    }

  self = g_new0(FilterInList, 1);
  filter_expr_node_init_instance(&self->super);
  self->value_handle = log_msg_get_value_handle(property);
  self->tree = g_tree_new((GCompareFunc) strcmp);

  while (getline(&line, &n, stream) != -1)
    {
      line[strlen(line) - 1] = '\0';
      if (line[0])
        g_tree_insert(self->tree, line, GINT_TO_POINTER(1));
      line = NULL;
    }
  fclose(stream);

  self->super.eval = filter_in_list_eval;
  self->super.free_fn = filter_in_list_free;
  return &self->super;
}
