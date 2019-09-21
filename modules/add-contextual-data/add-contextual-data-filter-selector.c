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

#include "add-contextual-data-filter-selector.h"
#include "syslog-ng.h"
#include "cfg.h"
#include "messages.h"
#include "filter/filter-expr.h"
#include "filter/filter-expr-parser.h"
#include "filter/filter-pipe.h"

typedef struct _FilterStore
{
  GList *filters;
  GList *filter_names;
} FilterStore;

typedef struct _AddContextualDataFilterSelector
{
  AddContextualDataSelector super;
  gchar *filters_path;
  GlobalConfig *master_cfg;
  GlobalConfig *filters_cfg;
  FilterStore *filter_store;
} AddContextualDataFilterSelector;

static FilterStore *
_filter_store_new(void)
{
  FilterStore *fs = g_new0(FilterStore, 1);
  return fs;
}

static void
_filter_store_free(FilterStore *self)
{
  g_list_free(self->filters);
  g_list_free(self->filter_names);
  g_free(self);
}

FilterStore *
_filter_store_clone(FilterStore *self)
{
  FilterStore *cloned = _filter_store_new();
  cloned->filters = g_list_copy(self->filters);
  cloned->filter_names = g_list_copy(self->filter_names);
  return cloned;
}

static void
_filter_store_prepend(FilterStore *self, gchar *name, FilterExprNode *filter)
{
  self->filters = g_list_prepend(self->filters, filter);
  self->filter_names = g_list_prepend(self->filter_names, name);
}

static const gchar *
_filter_store_get_first_matching_name(FilterStore *self, LogMessage *msg)
{
  GList *filter_it, *name_it;
  FilterExprNode *filter;
  const gchar *name = NULL;

  for (filter_it = self->filters, name_it = self->filter_names;
       filter_it != NULL && name_it != NULL;
       filter_it = filter_it->next, name_it = name_it->next)
    {
      filter = (FilterExprNode *) filter_it->data;
      msg_debug("Evaluating filter", evt_tag_str("filter_name", name_it->data));
      if (filter_expr_eval(filter, msg))
        {
          name = (const gchar *) name_it->data;
          break;
        }
    }

  return name;
}

static gboolean
_init_filters_from_file(AddContextualDataFilterSelector *self)
{
  self->filters_cfg = cfg_new_subordinate(self->master_cfg);
  if (!cfg_read_config(self->filters_cfg, self->filters_path, NULL))
    {
      cfg_free(self->filters_cfg);
      self->filters_cfg = NULL;
      msg_error("Error parsing filters of rule engine", evt_tag_str(EVT_TAG_FILENAME, self->filters_path));
      return FALSE;
    }
  return TRUE;
}

static FilterExprNode *
_init_filter_from_log_node(GlobalConfig *cfg, LogExprNode *node)
{
  LogFilterPipe *filter_pipe = (LogFilterPipe *) node->children->object;
  FilterExprNode *selected_filter = filter_expr_ref(filter_pipe->expr);

  filter_expr_init(selected_filter, cfg);

  return selected_filter;
}

static gboolean
_populate_filter_store(AddContextualDataFilterSelector *self)
{
  GList *objects_in_cfg = cfg_tree_get_objects(&self->filters_cfg->tree);
  GList *cfg_object;
  for (cfg_object = objects_in_cfg; cfg_object != NULL; cfg_object = cfg_object->next)
    {
      LogExprNode *node = (LogExprNode *) cfg_object->data;
      if (node->content != ENC_FILTER)
        {
          msg_error("Error populating filter store; non-filter object in config");
          g_list_free(objects_in_cfg);
          return FALSE;
        }

      FilterExprNode *selected_filter = _init_filter_from_log_node(self->filters_cfg, node);
      msg_debug("Insert into filter store", evt_tag_str("filter", node->name));
      _filter_store_prepend(self->filter_store, node->name, selected_filter);
    }

  g_list_free(objects_in_cfg);
  return TRUE;
}

static FilterStore *
_filter_store_order_by_selectors(FilterStore *self, GList *ordered_selectors)
{
  FilterStore *fs_ordered = _filter_store_new();
  GList *name_it, *filter_it, *selector_it;
  gboolean inserted;

  for(selector_it = ordered_selectors; selector_it != NULL; selector_it = selector_it->next)
    {
      inserted = FALSE;
      for (filter_it = self->filters, name_it = self->filter_names;
           filter_it != NULL && name_it != NULL;
           filter_it = filter_it->next, name_it = name_it->next)
        {
          if (g_strcmp0((const gchar *)selector_it->data, (const gchar *)name_it->data) == 0)
            {
              inserted = TRUE;
              _filter_store_prepend(fs_ordered, name_it->data, filter_it->data);
              self->filters = g_list_delete_link(self->filters, filter_it);
              self->filter_names = g_list_delete_link(self->filter_names, name_it);
              break;
            }
        }
      if (!inserted)
        msg_warning("A filter referenced by the database is not found in the filters file",
                    evt_tag_str("filter", selector_it->data));
    }
  fs_ordered->filters = g_list_reverse(fs_ordered->filters);
  fs_ordered->filter_names = g_list_reverse(fs_ordered->filter_names);
  _filter_store_free(self);

  return fs_ordered;
}

static gboolean
_init(AddContextualDataSelector *s, GList *ordered_selectors)
{
  AddContextualDataFilterSelector *self = (AddContextualDataFilterSelector *)s;

  if (!_init_filters_from_file(self))
    return FALSE;

  if (!_populate_filter_store(self))
    return FALSE;

  self->filter_store = _filter_store_order_by_selectors(self->filter_store, ordered_selectors);

  return TRUE;
}

static gchar *
_resolve(AddContextualDataSelector *s, LogMessage *msg)
{
  AddContextualDataFilterSelector *self = (AddContextualDataFilterSelector *)s;
  return g_strdup(_filter_store_get_first_matching_name(self->filter_store, msg));
}

static void
_free(AddContextualDataSelector *s)
{
  AddContextualDataFilterSelector *self = (AddContextualDataFilterSelector *)s;
  if (self->filters_cfg)
    cfg_free(self->filters_cfg);
  _filter_store_free(self->filter_store);
  g_free(self->filters_path);
}

static AddContextualDataSelector *_clone(AddContextualDataSelector *s,
                                         GlobalConfig *cfg);

static AddContextualDataFilterSelector *
_create_empty_add_contextual_data_filter_selector(void)
{
  AddContextualDataFilterSelector *self = g_new0(AddContextualDataFilterSelector, 1);
  self->super.ordering_required = TRUE;
  self->super.resolve = _resolve;
  self->super.free = _free;
  self->super.init = _init;
  self->super.clone = _clone;
  return self;
}

static AddContextualDataSelector *
_clone(AddContextualDataSelector *s, GlobalConfig *cfg)
{
  AddContextualDataFilterSelector *self = (AddContextualDataFilterSelector *)s;
  AddContextualDataFilterSelector *cloned = _create_empty_add_contextual_data_filter_selector();

  cloned->filters_path = g_strdup(self->filters_path);
  cloned->filters_cfg = NULL;
  cloned->master_cfg = self->master_cfg;
  cloned->filter_store  = _filter_store_clone(self->filter_store);

  return &cloned->super;
}

AddContextualDataSelector *
add_contextual_data_filter_selector_new(GlobalConfig *cfg, const gchar *filters_path)
{
  AddContextualDataFilterSelector *self = _create_empty_add_contextual_data_filter_selector();

  self->filters_path = g_strdup(filters_path);
  self->master_cfg = cfg;
  self->filters_cfg = NULL;
  self->filter_store = _filter_store_new();

  return &self->super;
}
