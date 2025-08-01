/*
 * Copyright (c) 2025 Romain Tartière
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

#include "filter-blank.h"

#include <ctype.h>

typedef struct _FilterBlank
{
  FilterExprNode super;
  const gchar *name;
  int name_len;
  gboolean blank;
} FilterBlank;

static gboolean
is_blank(const gchar *value, int value_len, NVType type)
{
  const gchar *c = value;
  gboolean res;

  switch (type)
    {
    case LM_VT_NULL:
    case LM_VT_NONE:
      return TRUE;
      break;
    case LM_VT_STRING:
      while (*c)
        {
          if (!isblank(*c))
            return FALSE;
          c++;
        }
      return TRUE;
      break;
    case LM_VT_BOOLEAN:
      if (type_cast_to_boolean(value, value_len, &res, NULL))
        return !res;
      return TRUE;
      break;
    case LM_VT_LIST:
      return value_len == 0;
      break;
    default:
      return FALSE;
    }
}

static gboolean
check_if_value_is_set(NVHandle handle, const gchar *name, const gchar *value, gssize value_len, NVType type, gpointer u)
{

  FilterBlank *user_data = (FilterBlank *)u;

  if (strncmp(name, user_data->name, user_data->name_len) == 0)
    {
      if (name[user_data->name_len] == '.')
        {
          // We have a composite value, so the field exist
          user_data->blank = FALSE;
          return TRUE;
        }
      else if (name[user_data->name_len] == '\0')
        {
          // This is a terminal value, test if it is blank
          user_data->blank = is_blank(value, value_len, type);
          return TRUE;
        }
    }
  return FALSE;
}

static gboolean
filter_blank_eval(FilterExprNode *s, LogMessage **msgs, gint num_msg, LogTemplateEvalOptions *options)
{
  FilterBlank *self = (FilterBlank *) s;

  LogMessage *msg = msgs[num_msg - 1];

  self->blank = TRUE;
  log_msg_values_foreach(msg, check_if_value_is_set, self);
  return self->blank ^ s->comp;
}

FilterExprNode *
filter_blank_new(const gchar *name)
{
  FilterBlank *self = g_new0(FilterBlank, 1);
  filter_expr_node_init_instance(&self->super);
  self->super.eval = filter_blank_eval;
  self->name = name;
  self->name_len = strlen(name);

  return &self->super;
}
