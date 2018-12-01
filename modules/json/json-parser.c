/*
 * Copyright (c) 2011-2014 Balabit
 * Copyright (c) 2011-2014 Gergely Nagy <algernon@balabit.hu>
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

#define JSON_C_VER_013 (13 << 8)

#include "json-parser.h"
#include "dot-notation.h"
#include "scratch-buffers.h"

#include <string.h>
#include <ctype.h>

#include <json.h>

#if (!defined(JSON_C_VERSION_NUM)) || (JSON_C_VERSION_NUM < JSON_C_VER_013)
#include <json_object_private.h>
#endif

typedef struct _JSONParser
{
  LogParser super;
  gchar *prefix;
  gchar *marker;
  gint marker_len;
  gchar *extract_prefix;
} JSONParser;

void
json_parser_set_prefix(LogParser *p, const gchar *prefix)
{
  JSONParser *self = (JSONParser *)p;

  g_free(self->prefix);
  self->prefix = g_strdup(prefix);
}

void
json_parser_set_marker(LogParser *p, const gchar *marker)
{
  JSONParser *self = (JSONParser *)p;

  g_free(self->marker);
  self->marker = g_strdup(marker);
  self->marker_len = marker ? strlen(marker) : 0;
}

void
json_parser_set_extract_prefix(LogParser *s, const gchar *extract_prefix)
{
  JSONParser *self = (JSONParser *) s;

  g_free(self->extract_prefix);
  self->extract_prefix = g_strdup(extract_prefix);
}

static void
json_parser_process_object(struct json_object *jso,
                           const gchar *prefix,
                           LogMessage *msg);

static void
json_parser_process_single(struct json_object *jso,
                           const gchar *prefix,
                           const gchar *obj_key,
                           LogMessage *msg)
{
  GString *key, *value;
  gboolean parsed = FALSE;

  if (!jso)
    return;

  ScratchBuffersMarker marker;
  key = scratch_buffers_alloc_and_mark(&marker);
  value = scratch_buffers_alloc();

  switch (json_object_get_type(jso))
    {
    case json_type_boolean:
      parsed = TRUE;
      if (json_object_get_boolean(jso))
        g_string_assign(value, "true");
      else
        g_string_assign(value, "false");
      break;
    case json_type_double:
      parsed = TRUE;
      g_string_printf(value, "%f",
                      json_object_get_double(jso));
      break;
    case json_type_int:
      parsed = TRUE;
      g_string_printf(value, "%i",
                      json_object_get_int(jso));
      break;
    case json_type_string:
      parsed = TRUE;
      g_string_assign(value,
                      json_object_get_string(jso));
      break;
    case json_type_object:
      if (prefix)
        g_string_assign(key, prefix);
      g_string_append(key, obj_key);
      g_string_append_c(key, '.');
      json_parser_process_object(jso, key->str, msg);
      break;
    case json_type_array:
    {
      gint i, plen;

      g_string_assign(key, obj_key);

      plen = key->len;

      for (i = 0; i < json_object_array_length(jso); i++)
        {
          g_string_truncate(key, plen);
          g_string_append_printf(key, "[%d]", i);
          json_parser_process_single(json_object_array_get_idx(jso, i),
                                     prefix,
                                     key->str, msg);
        }
      break;
    }
    case json_type_null:
      break;
    default:
      msg_error("JSON parser encountered an unknown type, skipping",
                evt_tag_str("key", obj_key));
      break;
    }

  if (parsed)
    {
      if (prefix)
        {
          g_string_assign(key, prefix);
          g_string_append(key, obj_key);
          log_msg_set_value_by_name(msg,
                                    key->str,
                                    value->str,
                                    value->len);
        }
      else
        log_msg_set_value_by_name(msg,
                                  obj_key,
                                  value->str,
                                  value->len);
    }

  scratch_buffers_reclaim_marked(marker);
}

static void
json_parser_process_object(struct json_object *jso,
                           const gchar *prefix,
                           LogMessage *msg)
{
  struct json_object_iter itr;

  json_object_object_foreachC(jso, itr)
  {
    json_parser_process_single(itr.val, prefix, itr.key, msg);
  }
}

static gboolean
json_parser_extract(JSONParser *self, struct json_object *jso, LogMessage *msg)
{
  if (self->extract_prefix)
    jso = json_extract(jso, self->extract_prefix);

  if (!jso || !json_object_is_type(jso, json_type_object))
    {
      return FALSE;
    }

  json_parser_process_object(jso, self->prefix, msg);
  return TRUE;
}

#ifndef JSON_C_VERSION
const char *
json_tokener_error_desc(enum json_tokener_error err)
{
  return json_tokener_errors[err];
}
#endif

static gboolean
json_parser_process(LogParser *s, LogMessage **pmsg, const LogPathOptions *path_options, const gchar *input,
                    gsize input_len)
{
  JSONParser *self = (JSONParser *) s;
  struct json_object *jso;
  struct json_tokener *tok;

  msg_trace("json-parser message processing started",
            evt_tag_str ("input", input),
            evt_tag_str ("prefix", self->prefix),
            evt_tag_str ("marker", self->marker),
            evt_tag_printf("msg", "%p", *pmsg));
  if (self->marker)
    {
      if (strncmp(input, self->marker, self->marker_len) != 0)
        {
          msg_debug("json-parser(): no marker at the beginning of the message, skipping JSON parsing ",
                    evt_tag_str ("input", input),
                    evt_tag_str ("marker", self->marker));
          return FALSE;
        }
      input += self->marker_len;

      while (isspace(*input))
        input++;
    }

  tok = json_tokener_new();
  jso = json_tokener_parse_ex(tok, input, input_len);
  if (tok->err != json_tokener_success || !jso)
    {
      msg_error("json-parser(): failed to parse JSON payload",
                evt_tag_str ("input", input),
                tok->err != json_tokener_success ? evt_tag_str ("json_error", json_tokener_error_desc(tok->err)) : NULL);
      json_tokener_free (tok);
      return FALSE;
    }
  json_tokener_free(tok);

  log_msg_make_writable(pmsg, path_options);
  if (!json_parser_extract(self, jso, *pmsg))
    {
      msg_error("json-parser(): failed to extract JSON members into name-value pairs. The parsed/extracted JSON payload was not an object",
                evt_tag_str("input", input),
                evt_tag_str("extract_prefix", self->extract_prefix));
      json_object_put(jso);
      return FALSE;
    }
  json_object_put(jso);

  return TRUE;
}

static LogPipe *
json_parser_clone(LogPipe *s)
{
  JSONParser *self = (JSONParser *) s;
  LogParser *cloned;

  cloned = json_parser_new(s->cfg);
  json_parser_set_prefix(cloned, self->prefix);
  json_parser_set_marker(cloned, self->marker);
  json_parser_set_extract_prefix(cloned, self->extract_prefix);
  log_parser_set_template(cloned, log_template_ref(self->super.template));

  return &cloned->super;
}

static void
json_parser_free(LogPipe *s)
{
  JSONParser *self = (JSONParser *)s;

  g_free(self->prefix);
  g_free(self->marker);
  g_free(self->extract_prefix);
  log_parser_free_method(s);
}

LogParser *
json_parser_new(GlobalConfig *cfg)
{
  JSONParser *self = g_new0(JSONParser, 1);

  log_parser_init_instance(&self->super, cfg);
  self->super.super.free_fn = json_parser_free;
  self->super.super.clone = json_parser_clone;
  self->super.process = json_parser_process;

  return &self->super;
}
