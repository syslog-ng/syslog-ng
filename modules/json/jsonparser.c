/*
 * Copyright (c) 2011-2013 BalaBit IT Ltd, Budapest, Hungary
 * Copyright (c) 2011-2013 Gergely Nagy <algernon@balabit.hu>
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
 */

#include "jsonparser.h"
#include "scratch-buffers.h"

#include <string.h>
#include <ctype.h>

#include <json.h>
#include <json_object_private.h>

struct _LogJSONParser
{
  LogParser super;
  gchar *prefix;
  gchar *marker;
  gint marker_len;
};

void
log_json_parser_set_prefix (LogParser *p, const gchar *prefix)
{
  LogJSONParser *self = (LogJSONParser *)p;

  g_free (self->prefix);
  self->prefix = g_strdup (prefix);
}

void
log_json_parser_set_marker (LogParser *p, const gchar *marker)
{
  LogJSONParser *self = (LogJSONParser *)p;

  g_free (self->marker);
  self->marker = g_strdup (marker);
  self->marker_len = strlen(marker);
}

static void
log_json_parser_process_object (struct json_object *jso,
                                const gchar *prefix,
                                LogMessage *msg);

static void
log_json_parser_process_single (struct json_object *jso,
                                const gchar *prefix,
                                const gchar *obj_key,
                                LogMessage *msg)
{
  SBGString *key, *value;
  gboolean parsed = FALSE;

  key = sb_gstring_acquire ();
  value = sb_gstring_acquire ();

  switch (json_object_get_type (jso))
    {
    case json_type_boolean:
      parsed = TRUE;
      if (json_object_get_boolean (jso))
        g_string_assign (sb_gstring_string (value), "true");
      else
        g_string_assign (sb_gstring_string (value), "false");
      break;
    case json_type_double:
      parsed = TRUE;
      g_string_printf (sb_gstring_string (value), "%f",
                       json_object_get_double (jso));
      break;
    case json_type_int:
      parsed = TRUE;
      g_string_printf (sb_gstring_string (value), "%i",
                       json_object_get_int (jso));
      break;
    case json_type_string:
      parsed = TRUE;
      g_string_assign (sb_gstring_string (value),
                       json_object_get_string (jso));
      break;
    case json_type_object:
      if (prefix)
        g_string_assign (sb_gstring_string (key), prefix);
      g_string_append (sb_gstring_string (key), obj_key);
      g_string_append_c (sb_gstring_string (key), '.');
      log_json_parser_process_object (jso, sb_gstring_string (key)->str, msg);
      break;
    case json_type_array:
      {
        gint i, plen;

        g_string_assign (sb_gstring_string (key), obj_key);

        plen = sb_gstring_string (key)->len;

        for (i = 0; i < json_object_array_length (jso); i++)
          {
            g_string_truncate (sb_gstring_string (key), plen);
            g_string_append_printf (sb_gstring_string (key), "[%d]", i);
            log_json_parser_process_single (json_object_array_get_idx (jso, i),
                                            prefix,
                                            sb_gstring_string (key)->str, msg);
          }
        break;
      }
    default:
      msg_error ("JSON parser encountered an unknown type, skipping",
                 evt_tag_str ("key", obj_key), NULL);
      break;
    }

  if (parsed)
    {
      if (prefix)
        {
          g_string_assign (sb_gstring_string (key), prefix);
          g_string_append (sb_gstring_string (key), obj_key);
          log_msg_set_value (msg,
                             log_msg_get_value_handle (sb_gstring_string (key)->str),
                             sb_gstring_string (value)->str,
                             sb_gstring_string (value)->len);
        }
      else
        log_msg_set_value (msg,
                           log_msg_get_value_handle (obj_key),
                           sb_gstring_string (value)->str,
                           sb_gstring_string (value)->len);
    }

  sb_gstring_release (key);
  sb_gstring_release (value);
}

static void
log_json_parser_process_object (struct json_object *jso,
                                const gchar *prefix,
                                LogMessage *msg)
{
  struct json_object_iter itr;

  json_object_object_foreachC (jso, itr)
    {
      log_json_parser_process_single (itr.val, prefix, itr.key, msg);
    }
}

static gboolean
log_json_parser_process (LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input, gsize input_len)
{
  LogJSONParser *self = (LogJSONParser *) s;
  struct json_object *jso;
  struct json_tokener *tok;

  if (self->marker)
    {
      if (strncmp(input, self->marker, self->marker_len) != 0)
        return FALSE;
      input += self->marker_len;

      while (isspace(*input))
        input++;
    }

  tok = json_tokener_new ();
  jso = json_tokener_parse_ex (tok, input, input_len);
  if (tok->err != json_tokener_success)
    {
      msg_error ("Unparsable JSON stream encountered",
                 evt_tag_str ("error", json_tokener_errors[tok->err]), NULL);
      json_tokener_free (tok);
      return FALSE;
    }
  json_tokener_free (tok);

  if (!jso)
    {
      msg_error ("Unparsable JSON stream encountered", NULL);
      return FALSE;
    }

  log_msg_make_writable(pmsg, path_options);
  log_json_parser_process_object (jso, self->prefix, *pmsg);

  json_object_put (jso);

  return TRUE;
}

static LogPipe *
log_json_parser_clone (LogPipe *s)
{
  LogJSONParser *self = (LogJSONParser *) s;
  LogJSONParser *cloned;

  cloned = (LogJSONParser *) log_json_parser_new ();
  log_json_parser_set_prefix ((LogParser *)cloned, self->prefix);
  log_json_parser_set_marker ((LogParser *)cloned, self->marker);

  return &cloned->super.super;
}

static void
log_json_parser_free (LogPipe *s)
{
  LogJSONParser *self = (LogJSONParser *)s;

  g_free (self->prefix);
  g_free (self->marker);
  log_parser_free_method (s);
}

LogJSONParser *
log_json_parser_new (void)
{
  LogJSONParser *self = g_new0 (LogJSONParser, 1);

  log_parser_init_instance (&self->super);
  self->super.super.free_fn = log_json_parser_free;
  self->super.super.clone = log_json_parser_clone;
  self->super.process = log_json_parser_process;

  return self;
}
