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
#include "filterx-globals.h"
#include "utf8utils.h"
#include "str-format.h"

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

const gchar *
filterx_bytes_get_value(FilterXObject *s, gsize *length)
{
  FilterXString *self = (FilterXString *) s;

  if (!filterx_object_is_type(s, &FILTERX_TYPE_NAME(bytes)))
    return NULL;

  g_assert(length);
  *length = self->str_len;

  return self->str;
}

const gchar *
filterx_protobuf_get_value(FilterXObject *s, gsize *length)
{
  FilterXString *self = (FilterXString *) s;

  if (!filterx_object_is_type(s, &FILTERX_TYPE_NAME(protobuf)))
    return NULL;

  g_assert(length);
  *length = self->str_len;

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
_len(FilterXObject *s, guint64 *len)
{
  FilterXString *self = (FilterXString *) s;

  *len = self->str_len;
  return TRUE;
}

static gboolean
_map_to_json(FilterXObject *s, struct json_object **object, FilterXObject **assoc_object)
{
  FilterXString *self = (FilterXString *) s;

  *object = json_object_new_string_len(self->str, self->str_len);
  return TRUE;
}

static gboolean
_string_repr(FilterXObject *s, GString *repr)
{
  FilterXString *self = (FilterXString *) s;
  repr = g_string_append_len(repr, self->str, self->str_len);
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
_bytes_map_to_json(FilterXObject *s, struct json_object **object, FilterXObject **assoc_object)
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

static gboolean
_bytes_repr(FilterXObject *s, GString *repr)
{
  FilterXString *self = (FilterXString *) s;

  gsize target_len = self->str_len * 2;
  gsize repr_len = repr->len;
  g_string_set_size(repr, target_len + repr_len);
  format_hex_string_with_delimiter(self->str, self->str_len, repr->str + repr_len, target_len, 0);
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

FilterXObject *
filterx_typecast_string(GPtrArray *args)
{
  FilterXObject *object = filterx_typecast_get_arg(args, NULL);
  if (!object)
    return NULL;

  if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(string)))
    {
      filterx_object_ref(object);
      return object;
    }

  GString *buf = scratch_buffers_alloc();

  if (!filterx_object_repr(object, buf))
    {
      msg_error("filterx: unable to repr",
                evt_tag_str("from", object->type->name),
                evt_tag_str("to", "string"));
      return NULL;
    }

  return filterx_string_new(buf->str, -1);
}

FilterXObject *
filterx_typecast_bytes(GPtrArray *args)
{
  FilterXObject *object = filterx_typecast_get_arg(args, NULL);
  if (!object)
    return NULL;

  if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(bytes)))
    {
      filterx_object_ref(object);
      return object;
    }

  if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(string)))
    {
      gsize size;
      const gchar *data = filterx_string_get_value(object, &size);
      return filterx_bytes_new(data, size);
    }

  if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(protobuf)))
    {
      gsize size;
      const gchar *data = filterx_protobuf_get_value(object, &size);
      return filterx_bytes_new(data, size);
    }

  msg_error("filterx: invalid typecast",
            evt_tag_str("from", object->type->name),
            evt_tag_str("to", "bytes"));
  return NULL;
}

FilterXObject *
filterx_typecast_protobuf(GPtrArray *args)
{
  FilterXObject *object = filterx_typecast_get_arg(args, NULL);
  if (!object)
    return NULL;

  if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(protobuf)))
    {
      filterx_object_ref(object);
      return object;
    }

  if (filterx_object_is_type(object, &FILTERX_TYPE_NAME(bytes)))
    {
      gsize size;
      const gchar *data = filterx_bytes_get_value(object, &size);
      return filterx_protobuf_new(data, size);
    }

  msg_error("filterx: invalid typecast",
            evt_tag_str("from", object->type->name),
            evt_tag_str("to", "protobuf"));

  return NULL;
}


/* these types are independent type-wise but share a lot of the details */

FILTERX_DEFINE_TYPE(string, FILTERX_TYPE_NAME(object),
                    .marshal = _marshal,
                    .len = _len,
                    .map_to_json = _map_to_json,
                    .truthy = _truthy,
                    .repr = _string_repr,
                   );

FILTERX_DEFINE_TYPE(bytes, FILTERX_TYPE_NAME(object),
                    .marshal = _bytes_marshal,
                    .len = _len,
                    .map_to_json = _bytes_map_to_json,
                    .truthy = _truthy,
                    .repr = _bytes_repr,
                   );

FILTERX_DEFINE_TYPE(protobuf, FILTERX_TYPE_NAME(object),
                    .len = _len,
                    .marshal = _bytes_marshal,
                    .map_to_json = _bytes_map_to_json,
                    .truthy = _truthy,
                    .repr = _bytes_repr,
                   );
