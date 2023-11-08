/*
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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
#include "object-string.h"
#include "str-utils.h"
#include "scratch-buffers.h"

typedef struct _FilterXString
{
  FilterXObject super;
  gsize str_len;
  gchar str[];
} FilterXString;

const gchar *
filterx_string_get_value(FilterXObject *s, gsize *length)
{
  FilterXString *self = (FilterXString *) s;

  if (!filterx_object_is_type(s, &FILTERX_TYPE_NAME(string)))
    return NULL;

  if (length)
    *length = self->str_len;
  else
    g_assert(self->str[self->str_len] == 0);
  return self->str;
}

static gboolean
_truthy(FilterXObject *s)
{
  FilterXString *self = (FilterXString *) s;
  return self->str_len > 0;
}

static gboolean
_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXString *self = (FilterXString *) s;

  g_string_append_len(repr, self->str, self->str_len);
  *t = LM_VT_STRING;
  return TRUE;
}

static gboolean
_map_to_json(FilterXObject *s, struct json_object **object)
{
  FilterXString *self = (FilterXString *) s;

  *object = json_object_new_string_len(self->str, self->str_len);
  return TRUE;
}

FilterXObject *
filterx_string_new(const gchar *str, gssize str_len)
{
  if (str_len < 0)
    str_len = strlen(str);
  FilterXString *self = g_malloc(sizeof(FilterXString) + str_len + 1);
  memset(self, 0, sizeof(FilterXString));
  filterx_object_init_instance(&self->super, &FILTERX_TYPE_NAME(string));
  self->str_len = str_len;
  memcpy(self->str, str, str_len);
  self->str[str_len] = 0;
  return &self->super;
}

static inline gsize
_get_base64_encoded_size(gsize len)
{
  return (len / 3 + 1) * 4 + 4;
}

static gboolean
_bytes_map_to_json(FilterXObject *s, struct json_object **object)
{
  FilterXString *self = (FilterXString *) s;
  GString *encode_buffer = scratch_buffers_alloc();

  gint encode_state = 0;
  gint encode_save = 0;

  /* expand the buffer and add space for the base64 encoded string */
  g_string_set_size(encode_buffer, _get_base64_encoded_size(self->str_len));
  gsize out_len = g_base64_encode_step((const guchar *) self->str, self->str_len, FALSE, encode_buffer->str,
                                       &encode_state, &encode_save);
  g_string_set_size(encode_buffer, out_len + _get_base64_encoded_size(0));

#if !GLIB_CHECK_VERSION(2, 54, 0)
  /* See modules/basicfuncs/str-funcs.c: tf_base64encode() */
  if (((unsigned char *) &encode_save)[0] == 1)
    ((unsigned char *) &encode_save)[2] = 0;
#endif

  out_len += g_base64_encode_close(FALSE, encode_buffer->str + out_len, &encode_state, &encode_save);
  g_string_set_size(encode_buffer, out_len);

  *object = json_object_new_string_len(encode_buffer->str, encode_buffer->len);
  return TRUE;
}

static gboolean
_bytes_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXString *self = (FilterXString *) s;

  g_string_append_len(repr, self->str, self->str_len);
  *t = LM_VT_BYTES;
  return TRUE;
}

FilterXObject *
filterx_bytes_new(const gchar *mem, gssize mem_len)
{
  FilterXString *self = (FilterXString *) filterx_string_new(mem, mem_len);
  self->super.type = &FILTERX_TYPE_NAME(bytes);
  return &self->super;
}

FilterXObject *
filterx_protobuf_new(const gchar *mem, gssize mem_len)
{
  FilterXString *self = (FilterXString *) filterx_bytes_new(mem, mem_len);
  self->super.type = &FILTERX_TYPE_NAME(protobuf);
  return &self->super;
}

/* these types are independent type-wise but share a lot of the details */

FILTERX_DEFINE_TYPE(string, FILTERX_TYPE_NAME(object),
                    .marshal = _marshal,
                    .map_to_json = _map_to_json,
                    .truthy = _truthy,
                   );

FILTERX_DEFINE_TYPE(bytes, FILTERX_TYPE_NAME(object),
                    .marshal = _bytes_marshal,
                    .map_to_json = _bytes_map_to_json,
                    .truthy = _truthy,
                   );

FILTERX_DEFINE_TYPE(protobuf, FILTERX_TYPE_NAME(object),
                    .marshal = _bytes_marshal,
                    .map_to_json = _bytes_map_to_json,
                    .truthy = _truthy,
                   );
