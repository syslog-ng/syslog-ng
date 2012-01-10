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
#include "scratch-buffers.h"

#include <string.h>

#include <json.h>
#include <json_object_private.h>

struct _LogJSONParser
{
  LogParser super;
  gchar *prefix;
};

void
log_json_parser_set_prefix (LogParser *p, const gchar *prefix)
{
  LogJSONParser *self = (LogJSONParser *)p;

  g_free (self->prefix);
  self->prefix = g_strdup (prefix);
}

static void
log_json_parser_process_object (struct json_object *jso,
                                const gchar *prefix,
                                LogMessage *msg)
{
  struct json_object_iter itr;
  ScratchBuffer *key, *value;

  key = scratch_buffer_acquire ();
  value = scratch_buffer_acquire ();

  json_object_object_foreachC (jso, itr)
    {
      gboolean parsed = FALSE;

      switch (json_object_get_type (itr.val))
        {
        case json_type_boolean:
          parsed = TRUE;
          if (json_object_get_boolean (itr.val))
            g_string_assign (sb_string (value), "true");
          else
            g_string_assign (sb_string (value), "false");
          break;
        case json_type_double:
          parsed = TRUE;
          g_string_printf (sb_string (value), "%f",
                           json_object_get_double (itr.val));
          break;
        case json_type_int:
          parsed = TRUE;
          g_string_printf (sb_string (value), "%i",
                           json_object_get_int (itr.val));
          break;
        case json_type_string:
          parsed = TRUE;
          g_string_assign (sb_string (value),
                           json_object_get_string (itr.val));
          break;
        case json_type_object:
          g_string_assign (sb_string (key), prefix);
          g_string_append (sb_string (key), itr.key);
          g_string_append_c (sb_string (key), '.');
          log_json_parser_process_object (itr.val, sb_string (key)->str, msg);
          break;
        case json_type_array:
          msg_error ("JSON parser does not support arrays yet, "
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
          if (prefix)
            {
              g_string_assign (sb_string (key), prefix);
              g_string_append (sb_string (key), itr.key);
              log_msg_set_value (msg,
                                 log_msg_get_value_handle (sb_string (key)->str),
                                 sb_string (value)->str, sb_string (value)->len);
            }
          else
            log_msg_set_value (msg,
                               log_msg_get_value_handle (itr.key),
                               sb_string (value)->str, sb_string (value)->len);
        }
    }
  scratch_buffer_release (key);
  scratch_buffer_release (value);
}

static gboolean
log_json_parser_process (LogParser *s, LogMessage *msg, const gchar *input)
{
  LogJSONParser *self = (LogJSONParser *) s;
  struct json_object *jso;
  struct json_object_iter itr;

  jso = json_tokener_parse (input);

  if (!jso)
    {
      msg_error ("Unparsable JSON stream encountered", NULL);
      return FALSE;
    }

  log_json_parser_process_object (jso, self->prefix, msg);

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

  return &cloned->super.super;
}

static void
log_json_parser_free (LogPipe *s)
{
  LogJSONParser *self = (LogJSONParser *)s;

  g_free (self->prefix);
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
