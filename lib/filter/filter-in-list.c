/*
 * Copyright (c) 2013, 2014 Balabit
 * Copyright (c) 2013, 2014 Gergely Nagy <algernon@balabit.hu>
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
#include "logmsg/logmsg.h"
#include "str-utils.h"

#include <errno.h>
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
  LogMessage *msg = msgs[num_msg - 1];
  const gchar *value;
  gssize len = 0;

  value = log_msg_get_value(msg, self->value_handle, &len);
  APPEND_ZERO(value, value, len);

  gboolean result = (g_tree_lookup(self->tree, value) != NULL);
  msg_debug("  in-list() evaluation result",
            filter_result_tag(result),
            evt_tag_str("value", value),
            evt_tag_printf("msg", "%p", msg));

  return result ^ s->comp;
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
  gchar line[16384];

  stream = fopen(list_file, "r");
  if (!stream)
    {
      msg_error("Error opening in-list filter list file",
                evt_tag_str("file", list_file),
                evt_tag_error("errno"));
      return NULL;
    }

  self = g_new0(FilterInList, 1);
  filter_expr_node_init_instance(&self->super);
  self->value_handle = log_msg_get_value_handle(property);
  self->tree = g_tree_new_full((GCompareDataFunc)strcmp, NULL, g_free, NULL);

  while (fgets(line, sizeof(line), stream) != NULL)
    {
      line[strlen(line) - 1] = '\0';
      if (line[0])
        g_tree_insert(self->tree, g_strdup(line), GINT_TO_POINTER(1));
    }
  fclose(stream);

  self->super.eval = filter_in_list_eval;
  self->super.free_fn = filter_in_list_free;
  return &self->super;
}
