/*
 * Copyright (c) 2011 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2011 Gergely Nagy <algernon@balabit.hu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, or (at your option) any later version.
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
 */

#include "jsonparser.h"
#include "logparser.h"

#include <string.h>

#include <json.h>
#include <json_object_private.h>

struct _LogJSONParser
{
  LogParser super;
  gchar *prefix;

  json_tokener *tokener;
  struct
  {
    GString *key;
    GString *value;
  } serialized;
};

void
log_json_parser_set_prefix (LogParser *p, const gchar *prefix)
{
  LogJSONParser *self = (LogJSONParser *)p;

  g_free (self->prefix);
  self->prefix = g_strdup (prefix);
}

static gboolean
log_json_parser_process (LogParser *s, LogMessage *msg, const gchar *input)
{
  LogJSONParser *self = (LogJSONParser *) s;
  struct json_object *jso;
  struct json_object_iter itr;

  json_tokener_reset (self->tokener);
  jso = json_tokener_parse_ex (self->tokener, input, strlen (input));

  json_object_object_foreachC (jso, itr)
    {
      gboolean parsed = FALSE;

      switch (json_object_get_type (itr.val))
	{
	case json_type_boolean:
	  msg_info ("JSON parser does not support boolean types yet, skipping",
		    evt_tag_str ("key", itr.key), NULL);
	  break;
	case json_type_double:
	  parsed = TRUE;
	  g_string_printf (self->serialized.value, "%f",
			   json_object_get_double (itr.val));
	  break;
	case json_type_int:
	  parsed = TRUE;
	  g_string_printf (self->serialized.value, "%i",
			   json_object_get_int (itr.val));
	  break;
	case json_type_string:
	  parsed = TRUE;
	  g_string_assign (self->serialized.value,
			   json_object_get_string (itr.val));
	  break;
	case json_type_object:
	case json_type_array:
	  msg_error ("JSON parser does not support objects and arrays yet, "
		     "skipping",
		     evt_tag_str ("key", itr.key), NULL);
	  break;
	default:
	  msg_error ("JSON parser encountered an unknown type, skipping",
		     evt_tag_str ("key", itr.key), NULL);
	  break;
	}

      if (parsed)
	{
	  if (self->prefix)
	    g_string_printf (self->serialized.key, "%s%s",
			     self->prefix, itr.key);
	  else
	    g_string_assign (self->serialized.key, itr.key);
	  log_msg_set_value (msg,
			     log_msg_get_value_handle (self->serialized.key->str),
			     self->serialized.value->str,
			     self->serialized.value->len);
	}
    }
  json_object_put (jso);

  return TRUE;
}

static LogPipe *
log_json_parser_clone (LogProcessPipe *s)
{
  LogJSONParser *self = (LogJSONParser *) s;
  LogJSONParser *cloned;

  cloned = (LogJSONParser *) log_json_parser_new ();
  log_json_parser_set_prefix ((LogParser *)cloned, self->prefix);

  return &cloned->super.super.super;
}

static void
log_json_parser_free (LogPipe *s)
{
  LogJSONParser *self = (LogJSONParser *)s;

  g_free (self->prefix);
  g_string_free (self->serialized.key, TRUE);
  g_string_free (self->serialized.value, TRUE);

  json_tokener_free (self->tokener);
  log_parser_free_method (s);
}

LogJSONParser *
log_json_parser_new (void)
{
  LogJSONParser *self = g_new0 (LogJSONParser, 1);

  log_parser_init_instance (&self->super);
  self->super.super.super.free_fn = log_json_parser_free;
  self->super.super.clone = log_json_parser_clone;
  self->super.process = log_json_parser_process;

  self->tokener = json_tokener_new ();
  self->serialized.key = g_string_new (NULL);
  self->serialized.value = g_string_new (NULL);

  return self;
}
