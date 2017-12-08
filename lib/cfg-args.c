/*
 * Copyright (c) 2002-2013 Balabit
 * Copyright (c) 1998-2013 Balázs Scheidler
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
#include "cfg-args.h"
#include "messages.h"
#include "str-utils.h"
#include "str-repr/encode.h"

struct _CfgArgs
{
  gint ref_cnt;
  GHashTable *args;
};

/* token block args */

static void
cfg_args_validate_callback(gpointer k, gpointer v, gpointer user_data)
{
  CfgArgs *defs = ((gpointer *) user_data)[0];
  gchar **bad_key = (gchar **) &((gpointer *) user_data)[1];
  gchar **bad_value = (gchar **) &((gpointer *) user_data)[2];

  if ((*bad_key == NULL) && (!defs || cfg_args_get(defs, k) == NULL))
    {
      *bad_key = k;
      *bad_value = v;
    }
}

void
cfg_args_foreach(CfgArgs *self, GHFunc func, gpointer user_data)
{
  g_hash_table_foreach(self->args, func, user_data);
}

gboolean
cfg_args_validate(CfgArgs *self, CfgArgs *defs, const gchar *context)
{
  gpointer validate_params[] = { defs, NULL, NULL };

  cfg_args_foreach(self, cfg_args_validate_callback, validate_params);

  if (validate_params[1])
    {
      msg_error("Unknown argument",
                evt_tag_str("context", context),
                evt_tag_str("arg", validate_params[1]),
                evt_tag_str("value", validate_params[2]));
      return FALSE;
    }
  return TRUE;
}

static void
_resolve_unknown_blockargs_as_varargs(gpointer key, gpointer value, gpointer user_data)
{
  CfgArgs *defaults = ((gpointer *) user_data)[0];
  GString *varargs = ((gpointer *) user_data)[1];

  if (!defaults || cfg_args_get(defaults, key) == NULL)
    {
      g_string_append_printf(varargs, "%s(%s) ", (gchar *)key, (gchar *)value);
    }
}

gchar *
cfg_args_format_varargs(CfgArgs *self, CfgArgs *defaults)
{
  GString *varargs = g_string_new("");
  gpointer user_data[] = { defaults, varargs };

  cfg_args_foreach(self, _resolve_unknown_blockargs_as_varargs, user_data);
  return g_string_free(varargs, FALSE);
}

void
cfg_args_set(CfgArgs *self, const gchar *name, const gchar *value)
{
  g_hash_table_insert(self->args, __normalize_key(name), g_strdup(value));
}

const gchar *
cfg_args_get(CfgArgs *self, const gchar *name)
{
  const gchar *value = g_hash_table_lookup(self->args, name);

  if (!value)
    {
      gchar *normalized_name = __normalize_key(name);
      value = g_hash_table_lookup(self->args, normalized_name);
      g_free(normalized_name);
    }

  return value;
}

CfgArgs *
cfg_args_new(void)
{
  CfgArgs *self = g_new0(CfgArgs, 1);

  self->args = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  self->ref_cnt = 1;
  return self;
}

CfgArgs *
cfg_args_ref(CfgArgs *self)
{
  if (self)
    self->ref_cnt++;
  return self;
}

void
cfg_args_unref(CfgArgs *self)
{
  if (self && --self->ref_cnt == 0)
    {
      g_hash_table_destroy(self->args);
      g_free(self);
    }
}
