/*
 * Copyright (c) 2013 Balabit
 * Copyright (c) 2013 Balázs Scheidler
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

#include "filter-tags.h"
#include "logmsg/logmsg.h"

typedef struct _FilterTags
{
  FilterExprNode super;
  GArray *tags;
} FilterTags;

static gboolean
filter_tags_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterTags *self = (FilterTags *)s;
  LogMessage *msg = msgs[num_msg - 1];
  gboolean res;
  gint i;

  for (i = 0; i < self->tags->len; i++)
    {
      LogTagId tag_id = g_array_index(self->tags, LogTagId, i);
      if (log_msg_is_tag_by_id(msg, tag_id))
        {
          res = TRUE;
          msg_trace("tags() evaluation started",
                    evt_tag_str("tag", log_tags_get_by_id(tag_id)),
                    evt_tag_printf("msg", "%p", msg));
          return res ^ s->comp;
        }
    }

  res = FALSE;
  msg_trace("tags() evaluation started",
            evt_tag_printf("msg", "%p", msg));
  return res ^ s->comp;
}

void
filter_tags_add(FilterExprNode *s, GList *tags)
{
  FilterTags *self = (FilterTags *)s;
  LogTagId id;

  while (tags)
    {
      id = log_tags_get_by_name((gchar *) tags->data);
      g_free(tags->data);
      tags = g_list_delete_link(tags, tags);
      g_array_append_val(self->tags, id);
    }
}

static void
filter_tags_free(FilterExprNode *s)
{
  FilterTags *self = (FilterTags *)s;

  g_array_free(self->tags, TRUE);
}

FilterExprNode *
filter_tags_new(GList *tags)
{
  FilterTags *self = g_new0(FilterTags, 1);

  filter_expr_node_init_instance(&self->super);
  self->tags = g_array_new(FALSE, FALSE, sizeof(LogTagId));

  filter_tags_add(&self->super, tags);

  self->super.eval = filter_tags_eval;
  self->super.free_fn = filter_tags_free;
  self->super.type = g_strdup("tags");
  return &self->super;
}
