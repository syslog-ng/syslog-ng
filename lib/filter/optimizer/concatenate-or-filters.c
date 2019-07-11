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
#include "filter/optimizer/concatenate-or-filters.h"
#include "filter/filter-expr-parser.h"


static FilterExprNode *_compile_standalone_filter(gchar *config_snippet)
{
  GlobalConfig *cfg = cfg_new_snippet();
  CfgLexer *lexer = cfg_lexer_new_buffer(cfg, config_snippet, strlen(config_snippet));
  FilterExprNode *tmp;
  g_assert(cfg_run_parser(cfg, lexer, &filter_expr_parser, (gpointer *) &tmp, NULL));

  cfg_free(cfg);
  return tmp;
}

static FilterExprNode *
_concatenate(FilterExprNode *current, FilterExprNode *parent, FilterExprNode *left, FilterExprNode *right)
{
  GString *new_filter = g_string_new("");

  g_string_printf(new_filter, "%sprogram(\"%s|%s\");", (left->comp ? "not " : ""), left->pattern, right->pattern);

  FilterExprNode *new_opt = _compile_standalone_filter(new_filter->str);
  filter_expr_replace_child(parent, current, new_opt);
  return new_opt;
}




static gboolean
_can_we_concatenate(FilterExprNode *current, FilterExprNode *left, FilterExprNode *right)
{
  // Is it an OR filter
  if (strcmp(current->type, "OR") != 0)
    return FALSE;

  if (left->comp != right->comp)
    return FALSE;

  if (strcmp(left->type, right->type) !=0 )
    return FALSE;

  if (strcmp(left->template, right->template) !=0 )
    return FALSE;

  return TRUE;
}



static gpointer
_concatenate_or_filters_init(FilterExprNode *root)
{
  GList **stack = g_new0(GList *, 1);

  return stack;
}

static void
_concatenate_or_filters_deinit(gpointer cookie)
{
  GList **stack = (GList **)cookie;
  g_free(stack);
}

static void
_concatenate_or_filters_cb(FilterExprNode *current, FilterExprNode *parent, GPtrArray *childs, gpointer cookie)
{
  GList **stack = (GList **)cookie;

  GList *left_link = NULL;
  FilterExprNode *left = NULL;
  GList *right_link = NULL;
  FilterExprNode *right = NULL;
  if (strcmp(current->type, "OR") == 0)
    {
      left_link = g_list_last(*stack);
      right_link = g_list_last(*stack)->prev;
      left = (FilterExprNode *)left_link->data;
      right = (FilterExprNode *)right_link->data;
    }

  if (_can_we_concatenate(current, left, right))
    {
      *stack = g_list_append(*stack, _concatenate(current, parent, left, right));
    }
  else
    {
      *stack = g_list_append(*stack, current);
    }
}

FilterExprOptimizer concatenate_or_filters =
{
  .name = "concatenate-or-filters",
  .init = _concatenate_or_filters_init,
  .deinit = _concatenate_or_filters_deinit,
  .cb = _concatenate_or_filters_cb
};

FilterExprOptimizer *concatenate_or_filters_get_instance(void)
{
  return &concatenate_or_filters;
}
