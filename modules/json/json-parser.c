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
#include "str-repr/encode.h"

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
  gchar key_delimiter;
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

void
json_parser_set_key_delimiter(LogParser *s, gchar delimiter)
{
  JSONParser *self = (JSONParser *) s;

  self->key_delimiter = delimiter;
}

static void
json_parser_store_value(JSONParser *self,
                        const gchar *prefix, const gchar *obj_key,
                        GString *value, LogMessageValueType type,
                        LogMessage *msg)
{
  GString *key;

  key = scratch_buffers_alloc();
  if (prefix)
    {
      g_string_assign(key, prefix);
      g_string_append(key, obj_key);
      log_msg_set_value_by_name_with_type(msg, key->str, value->str, value->len, type);
    }
  else
    log_msg_set_value_by_name_with_type(msg, obj_key, value->str, value->len, type);
}

static void
json_parser_process_object(JSONParser *self,
                           struct json_object *jso,
                           const gchar *prefix,
                           LogMessage *msg);


static gboolean
json_parser_extract_string_from_simple_json_object(JSONParser *self,
                                                   struct json_object *jso,
                                                   GString *value,
                                                   LogMessageValueType *type)
{
  switch (json_object_get_type(jso))
    {
    case json_type_boolean:
      if (json_object_get_boolean(jso))
        g_string_assign(value, "true");
      else
        g_string_assign(value, "false");
      *type = LM_VT_BOOLEAN;
      return TRUE;
    case json_type_double:
      g_string_printf(value, "%f",
                      json_object_get_double(jso));
      *type = LM_VT_DOUBLE;
      return TRUE;
    case json_type_int:
      g_string_printf(value, "%"PRId64,
                      json_object_get_int64(jso));
      *type = LM_VT_INTEGER;
      return TRUE;
    case json_type_string:
      g_string_assign(value,
                      json_object_get_string(jso));
      *type = LM_VT_STRING;
      return TRUE;
    case json_type_null:

      /* null values are represented as an empty string (so when expands to
       * "nothing" in type unaware situations, while type-aware consumers
       * may reproduce it as a NULL).
       *
       * I was thinking about using a very specific string representation
       * (e.g.  "NULL" as a value), however if we encounter a null at a
       * place where we might also have a sub-object, and we explicitly
       * reference a field within that sub-object, we would get an empty
       * string in that case too (due to the key being unknown).  All in
       * all, the base case is I think to use an empty string as value, and
       * LM_VT_NULL as type.  Type aware consumers would ignore the string
       * anyway.
       */
      g_string_truncate(value, 0);
      *type = LM_VT_NULL;
      return TRUE;
    default:
      break;
    }
  return FALSE;
}

static gboolean
json_parser_extract_value_from_simple_json_object(JSONParser *self,
                                                  struct json_object *jso,
                                                  const gchar *prefix, const gchar *obj_key,
                                                  LogMessage *msg)
{
  GString *value = scratch_buffers_alloc();
  LogMessageValueType type = LM_VT_STRING;

  if (!json_parser_extract_string_from_simple_json_object(self, jso, value, &type))
    return FALSE;
  json_parser_store_value(self, prefix, obj_key, value, type, msg);
  return TRUE;
}

static gboolean
json_parser_extract_values_from_complex_json_object(JSONParser *self,
                                                    struct json_object *jso,
                                                    const gchar *prefix, const gchar *obj_key,
                                                    LogMessage *msg)
{
  switch (json_object_get_type(jso))
    {
    case json_type_object:
    {
      GString *key = scratch_buffers_alloc();
      if (prefix)
        g_string_assign(key, prefix);
      g_string_append(key, obj_key);
      g_string_append_c(key, self->key_delimiter);
      json_parser_process_object(self, jso, key->str, msg);
      return TRUE;
    }
    case json_type_array:
    {
      GString *value = scratch_buffers_alloc();
      LogMessageValueType type = LM_VT_LIST;

      g_string_truncate(value, 0);

      for (gint i = 0; i < json_object_array_length(jso); i++)
        {
          struct json_object *el = json_object_array_get_idx(jso, i);
          if (json_object_get_type(el) == json_type_string)
            {
              const gchar *element_value = json_object_get_string(el);
              if (i != 0)
                g_string_append_c(value, ',');
              str_repr_encode_append(value, element_value, -1, NULL);
            }
          else
            {
              /* unknown type, encode the entire array as JSON */
              g_string_assign(value, json_object_to_json_string_ext(jso, JSON_C_TO_STRING_PLAIN));
              type = LM_VT_JSON;
              break;
            }
        }

      json_parser_store_value(self, prefix, obj_key, value, type, msg);
      return TRUE;
    }
    default:
      break;
    }
  return FALSE;
}


static void
json_parser_process_attribute(JSONParser *self,
                              struct json_object *jso,
                              const gchar *prefix,
                              const gchar *obj_key,
                              LogMessage *msg)
{
  ScratchBuffersMarker marker;
  scratch_buffers_mark(&marker);

  if (!json_parser_extract_value_from_simple_json_object(self, jso, prefix, obj_key, msg) &&
      !json_parser_extract_values_from_complex_json_object(self, jso, prefix, obj_key, msg))
    {
      msg_debug("JSON parser encountered an unknown type, skipping",
                evt_tag_str("key", obj_key),
                evt_tag_int("type", json_object_get_type(jso)));
    }


  scratch_buffers_reclaim_marked(marker);
}

static void
json_parser_process_object(JSONParser *self,
                           struct json_object *jso,
                           const gchar *prefix,
                           LogMessage *msg)
{
  struct json_object_iter itr;

  json_object_object_foreachC(jso, itr)
  {
    json_parser_process_attribute(self, itr.val, prefix, itr.key, msg);
  }
}

static void
json_parser_process_array(JSONParser *self,
                          struct json_object *jso,
                          const gchar *prefix,
                          LogMessage *msg)
{
  gint i;

  log_msg_unset_match(msg, 0);
  for (i = 0; i < json_object_array_length(jso) && i < LOGMSG_MAX_MATCHES; i++)
    {
      struct json_object *el = json_object_array_get_idx(jso, i);
      GString *element_value = scratch_buffers_alloc();
      LogMessageValueType element_type;

      if (json_parser_extract_string_from_simple_json_object(self, el, element_value, &element_type))
        {
          log_msg_set_match_with_type(msg, i + 1, element_value->str, element_value->len, element_type);
        }
      else
        {
          /* unknown type, encode the entire value as JSON */
          log_msg_set_match_with_type(msg, i + 1, json_object_to_json_string_ext(el, JSON_C_TO_STRING_PLAIN), -1, LM_VT_JSON);
        }
    }
  log_msg_truncate_matches(msg, i + 1);
}

static gboolean
json_parser_extract(JSONParser *self, struct json_object *jso, LogMessage *msg)
{
  if (self->extract_prefix)
    jso = json_extract(jso, self->extract_prefix);

  if (!jso)
    return FALSE;

  if (json_object_is_type(jso, json_type_object))
    {
      json_parser_process_object(self, jso, self->prefix, msg);
      return TRUE;
    }
  if (json_object_is_type(jso, json_type_array))
    {
      json_parser_process_array(self, jso, self->prefix, msg);
      return TRUE;
    }
  return FALSE;
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
            evt_tag_str("input", input),
            evt_tag_str("prefix", self->prefix),
            evt_tag_str("marker", self->marker),
            evt_tag_msg_reference(*pmsg));
  if (self->marker)
    {
      if (strncmp(input, self->marker, self->marker_len) != 0)
        {
          msg_debug("json-parser(): no marker at the beginning of the message, skipping JSON parsing ",
                    evt_tag_str("input", input),
                    evt_tag_str("marker", self->marker));
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
      msg_debug("json-parser(): failed to parse JSON payload",
                evt_tag_str("input", input),
                tok->err != json_tokener_success ? evt_tag_str ("json_error", json_tokener_error_desc(tok->err)) : NULL);
      json_tokener_free (tok);
      return FALSE;
    }
  json_tokener_free(tok);

  log_msg_make_writable(pmsg, path_options);
  if (!json_parser_extract(self, jso, *pmsg))
    {
      msg_debug("json-parser(): failed to extract JSON members into name-value pairs. The parsed/extracted JSON payload was not an object",
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
  log_parser_clone_settings(&self->super, cloned);
  json_parser_set_prefix(cloned, self->prefix);
  json_parser_set_marker(cloned, self->marker);
  json_parser_set_extract_prefix(cloned, self->extract_prefix);
  json_parser_set_key_delimiter(cloned, self->key_delimiter);

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
  self->key_delimiter = '.';

  return &self->super;
}
