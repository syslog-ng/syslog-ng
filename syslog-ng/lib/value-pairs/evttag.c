/*
 * Copyright (c) 2011-2015 Balabit
 * Copyright (c) 2011-2014 Gergely Nagy <algernon@balabit.hu>
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
#include "value-pairs/evttag.h"

static gboolean
_append_pair_to_debug_string(const gchar *name, TypeHint type, const gchar *value, gsize value_len, gpointer user_data)
{
  GString *text = (GString *) user_data;
  g_string_append_printf(text, "%s=%s ", name, value);
  return FALSE;
}

EVTTAG *
evt_tag_value_pairs(const char *key, ValuePairs *vp, LogMessage *msg, gint32 seq_num, gint time_zone_mode,
                    LogTemplateOptions *template_options)
{
  GString *debug_text = g_string_new("");
  EVTTAG *result;

  value_pairs_foreach(vp, _append_pair_to_debug_string, msg, seq_num, time_zone_mode, template_options, debug_text);

  result = evt_tag_str(key, debug_text->str);

  g_string_free(debug_text, TRUE);
  return result;
}
