#include "filter-tags.h"
#include "logmsg.h"

typedef struct _FilterTags
{
  FilterExprNode super;
  GArray *tags;
} FilterTags;

static gboolean
filter_tags_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg)
{
  FilterTags *self = (FilterTags *)s;
  LogMessage *msg = msgs[0];
  gint i;

  for (i = 0; i < self->tags->len; i++)
    {
      if (log_msg_is_tag_by_id(msg, g_array_index(self->tags, LogTagId, i)))
        return TRUE ^ s->comp;
    }

  return FALSE ^ s->comp;
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
  return &self->super;
}
