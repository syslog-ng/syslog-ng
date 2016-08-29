/*
 * Copyright (c) 2014 Balabit
 * Copyright (c) 2014 Balazs Scheidler <bazsi@balabit.hu>
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
#include "dot-notation.h"
#include <stdlib.h>

typedef struct _JSONDotNotationElem
{
  gboolean used;

  enum
  {
    JS_MEMBER_REF,
    JS_ARRAY_REF
  } type;
  union
  {
    struct
    {
      gchar *name;
    } member_ref;
    struct
    {
      gint index;
    } array_ref;
  };
} JSONDotNotationElem;

typedef struct JSONDotNotation
{
  JSONDotNotationElem *compiled_elems;
} JSONDotNotation;

static void _free_compiled_dot_notation(JSONDotNotationElem *compiled);

static gboolean
_compile_dot_notation_array_ref(const gchar *level, JSONDotNotationElem *elem)
{
  const gchar *p = level;
  gint index_ = 0;

  g_assert(*p == '[');

  p++;
  index_ = strtol(p, (gchar **) &p, 10);

  if (*p != ']')
    return FALSE;
  if (index_ < 0)
    return FALSE;
  p++;

  if (*p != 0)
    return FALSE;

  elem->type = JS_ARRAY_REF;
  elem->array_ref.index = index_;
  return TRUE;
}

static gboolean
_compile_dot_notation_member_ref(const gchar *level, JSONDotNotationElem *elem)
{
  const gchar *p = level;

  if (!g_ascii_isprint(*p) || strchr(".[]", *p) != NULL)
    return FALSE;

  while (g_ascii_isprint(*p) && strchr(".[]", *p) == NULL)
    p++;

  if (*p != 0)
    return FALSE;

  elem->type = JS_MEMBER_REF;
  elem->member_ref.name = g_strdup(level);
  return TRUE;
}

static gboolean
_compile_dot_notation_elem(const gchar *level, GArray *compiled_array)
{
  JSONDotNotationElem elem;

  memset(&elem, 0, sizeof(elem));

  if (level[0] == '[')
    {
      if (!_compile_dot_notation_array_ref(level, &elem))
        return FALSE;
    }
  else
    {
      if (!_compile_dot_notation_member_ref(level, &elem))
        return FALSE;
    }

  elem.used = TRUE;
  g_array_append_val(compiled_array, elem);
  return TRUE;
}

static gchar **
_split_dot_notation(const gchar *dot_notation)
{
  GPtrArray *array;
  const gchar *p, *last;

  array = g_ptr_array_new();
  p = last = dot_notation;
  while (*p)
    {
      if (*p == '.')
        {
          g_ptr_array_add(array, g_strndup(last, p - last));
          p++;
          last = p;
        }
      else if (*p == '[')
        {
          g_ptr_array_add(array, g_strndup(last, p - last));
          last = p;
          p++;
        }
      else
        {
          p++;
        }
    }
  g_ptr_array_add(array, g_strndup(last, p - last));
  g_ptr_array_add(array, NULL);
  return (gchar **) g_ptr_array_free(array, FALSE);
}

static JSONDotNotationElem *
_compile_dot_notation(const gchar *dot_notation)
{
  GArray *compiled;
  gchar **levels;
  gint i;

  levels = _split_dot_notation(dot_notation);

  compiled = g_array_new(TRUE, TRUE, sizeof(JSONDotNotationElem));
  for (i = 0; levels[i]; i++)
    {
      if (i == 0 && strlen(levels[i]) == 0)
        continue;
      if (!_compile_dot_notation_elem(levels[i], compiled))
        goto error;
    }
  g_strfreev(levels);
  return (JSONDotNotationElem *) g_array_free(compiled, FALSE);

error:
  g_strfreev(levels);
  _free_compiled_dot_notation((JSONDotNotationElem *) g_array_free(compiled, FALSE));
  return NULL;
}

static void
_free_compiled_dot_notation(JSONDotNotationElem *compiled)
{
  gint i;

  i = -1;
  for (i = 0; compiled && compiled[i].used; i++)
    {
      if (compiled[i].type == JS_MEMBER_REF)
        g_free(compiled[i].member_ref.name);
    }
  g_free(compiled);
}

static gboolean
json_dot_notation_compile(JSONDotNotation *self, const gchar *dot_notation)
{
  if (dot_notation[0] == 0)
    {
      self->compiled_elems = NULL;
      return TRUE;
    }
  self->compiled_elems = _compile_dot_notation(dot_notation);
  return self->compiled_elems != NULL;
}

#ifdef JSON_C_VERSION
struct json_object *
_json_object_object_get(struct json_object *obj, const char *key)
{
  struct json_object *value;

  json_object_object_get_ex(obj, key, &value);
  return value;
}
#else
#define _json_object_object_get json_object_object_get
#endif

struct json_object *
json_dot_notation_eval(JSONDotNotation *self, struct json_object *jso)
{
  JSONDotNotationElem *compiled;
  gint i;

  if (!jso)
    goto error;

  compiled = self->compiled_elems;
  for (i = 0; compiled && compiled[i].used; i++)
    {
      if (1)
        {
          const gchar *name;

          if (compiled[i].type == JS_MEMBER_REF)
            {
              name = compiled[i].member_ref.name;

              if (!json_object_is_type(jso, json_type_object))
                {
                  jso = NULL;
                  goto error;
                }
              jso = _json_object_object_get(jso, name);
            }
          else if (compiled[i].type == JS_ARRAY_REF)
            {
              if (!json_object_is_type(jso, json_type_array) ||
                  compiled[i].array_ref.index >= json_object_array_length(jso))
                {
                  jso = NULL;
                  goto error;
                }
              jso = json_object_array_get_idx(jso, compiled[i].array_ref.index);
            }
        }
    }
error:
  return jso;
}

JSONDotNotation *
json_dot_notation_new(void)
{
  JSONDotNotation *self = g_new0(JSONDotNotation, 1);

  return self;
}

void
json_dot_notation_free(JSONDotNotation *self)
{
  _free_compiled_dot_notation(self->compiled_elems);
  g_free(self);
}

struct json_object *
json_extract(struct json_object *jso, const gchar *dot_notation)
{
  JSONDotNotation *self = json_dot_notation_new();

  if (!json_dot_notation_compile(self, dot_notation))
    {
      jso = NULL;
      goto error;
    }

  jso = json_dot_notation_eval(self, jso);
error:
  json_dot_notation_free(self);
  return jso;
}
