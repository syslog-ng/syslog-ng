/*
 * Copyright (c) 2019 Balabit
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

#include "state.h"
#include "misc.h"
#include "logmsg/logmsg.h"
#include "jsonc/json.h"
#include "rcptid.h"
#include "messages.h"

static GHashTable *persist_state_storage = NULL;

NameValueContainer *
name_value_container_new()
{
  NameValueContainer *self = g_new0(NameValueContainer, 1);
  self->container = json_object_new_object();
  return self;
}

gboolean
name_value_container_parse_json_string(NameValueContainer *self, gchar *json_string)
{
  struct json_object *new_container = json_tokener_parse(json_string);
  if (!new_container)
    {
      return FALSE;
    }
  json_object_put(self->container);
  self->container = new_container;
  return TRUE;
}

void
name_value_container_add_string(NameValueContainer *self, gchar *name,
                                gchar *value)
{
  json_object_object_add(self->container, name, json_object_new_string(value));
}

void
name_value_container_add_boolean(NameValueContainer *self, gchar *name,
                                 gboolean value)
{
  json_object_object_add(self->container, name, json_object_new_boolean(value));
}

void
name_value_container_add_int(NameValueContainer *self, gchar *name, gint value)
{
  json_object_object_add(self->container, name, json_object_new_int(value));
}
void
name_value_container_add_int64(NameValueContainer *self, gchar *name,
                               gint64 value)
{
  json_object_object_add(self->container, name, json_object_new_int64(value));
}

gboolean
name_value_container_is_name_exists(NameValueContainer *self, gchar *name)
{
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  return !!(value);
}

gchar *
name_value_container_get_value(NameValueContainer *self, gchar *name)
{
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value)
    {
      return g_strdup(json_object_get_string(value));
    }
  return NULL;
}

const gchar *
name_value_container_get_string(NameValueContainer *self, gchar *name)
{
  const gchar *result = NULL;
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value && (json_object_is_type(value, json_type_string)))
    {
      result = json_object_get_string(value);
    }
  return result;
}

gboolean
name_value_container_get_boolean(NameValueContainer *self, gchar *name)
{
  gboolean result = FALSE;
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value && (json_object_is_type(value, json_type_boolean)))
    {
      result = json_object_get_boolean(value);
    }
  return result;
}

gint
name_value_container_get_int(NameValueContainer *self, gchar *name)
{
  gint result = -1;
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value && (json_object_is_type(value, json_type_int)))
    {
      result = json_object_get_int(value);
    }
  return result;
}

gint64
name_value_container_get_int64(NameValueContainer *self, gchar *name)
{
  gint64 result = FALSE;
  struct json_object *value;
  value = json_object_object_get(self->container, name);
  if (value && (json_object_is_type(value, json_type_int)))
    {
      result = json_object_get_int64(value);
    }
  return result;
}

void
name_value_container_foreach(NameValueContainer *self, void
                             (callback)(gchar *name, const gchar *value, gpointer user_data), gpointer user_data)
{

  json_object_object_foreach(self->container, key, val)
  {
    callback(key, json_object_get_string(val), user_data);
  }
  return;
}

gchar *
name_value_container_get_json_string(NameValueContainer *self)
{
  return g_strdup(json_object_to_json_string(self->container));
}

void
name_value_container_free(NameValueContainer *self)
{
  json_object_put(self->container);
  g_free(self);
}

gboolean
state_handler_load_state(StateHandler *self, NameValueContainer *new_state, GError **error)
{
  if (self->load_state)
    {
      return self->load_state(self, new_state, error);
    }
  if (error)
    {
      *error = g_error_new(1, 1, "load_state isn't implemented for this state_handler");
    }
  return FALSE;
}

PersistEntryHandle
state_handler_get_handle(StateHandler *self)
{
  PersistEntryHandle result = 0;
  if (self->persist_handle == 0)
    {
      self->persist_handle = persist_state_lookup_entry(self->persist_state,
                                                        self->name, &self->size, &self->version);
    }
  result = self->persist_handle;
  return result;
}

gboolean
state_handler_entry_exists(StateHandler *self)
{
  return !!(persist_state_lookup_entry(self->persist_state, self->name,
                                       &self->size, &self->version));
}

gchar *
state_handler_get_persist_name(StateHandler *self)
{
  return self->name;
}

guint8
state_handler_get_persist_version(StateHandler *self)
{
  return self->version;
}

gpointer
state_handler_get_state(StateHandler *self)
{
  if (!self->state)
    {
      if (self->persist_handle == 0)
        {
          self->persist_handle = state_handler_get_handle(self);
        }
      if (self->persist_handle)
        {
          self->state = persist_state_map_entry(self->persist_state,
                                                self->persist_handle);
        }
    }
  return self->state;
}

PersistEntryHandle
state_handler_alloc_state(StateHandler *self)
{
  state_handler_put_state(self);
  self->persist_handle = 0;
  if (self->alloc_state)
    {
      self->persist_handle = self->alloc_state(self);
    }
  return self->persist_handle;
}

void
state_handler_put_state(StateHandler *self)
{
  if (self->persist_state && self->persist_handle && self->state != NULL)
    {
      persist_state_unmap_entry(self->persist_state, self->persist_handle);
    }
  self->state = NULL;
}

gboolean
state_handler_rename_entry(StateHandler *self, const gchar *new_name)
{
  state_handler_put_state(self);
  if (persist_state_rename_entry(self->persist_state, self->name, new_name))
    {
      g_free(self->name);
      self->name = g_strdup(new_name);
      return TRUE;
    }
  return FALSE;
}

NameValueContainer *
state_handler_dump_state(StateHandler *self)
{
  if (self->dump_state)
    {
      return self->dump_state(self);
    }
  return NULL;
}

void
state_handler_free(StateHandler *self)
{
  if (self == NULL)
    {
      return;
    }
  state_handler_put_state(self);
  if (self->free_fn)
    {
      self->free_fn(self);
    }
  g_free(self->name);
  g_free(self);
}

void
state_handler_init(StateHandler *self, PersistState *persist_state,
                   const gchar *name)
{
  self->persist_state = persist_state;
  self->name = g_strdup(name);
}

PersistEntryHandle
state_handler_rcptid_alloc_state(StateHandler *self)
{
  PersistEntryHandle result = 0;
  if (self->persist_state)
    {
      result = persist_state_alloc_entry(self->persist_state, self->name, sizeof(RcptidState));
    }
  return result;
}

gboolean
_state_handler_rcptid_state_is_valid(NameValueContainer *container)
{
  gboolean result = TRUE;
  result = result && name_value_container_is_name_exists(container, "version");
  result = result && name_value_container_is_name_exists(container, "big_endian");
  result = result && name_value_container_is_name_exists(container, "rcptid");
  return result;
}

gboolean
state_handler_rcptid_load_state(StateHandler *self, NameValueContainer *container, GError **error)
{
  RcptidState *new_state = NULL;
  if (!_state_handler_rcptid_state_is_valid(container))
    {
      if (error)
        {
          *error = g_error_new(1, 2, "Some member is missing from the given state");
        }
      return FALSE;
    }
  if (!state_handler_entry_exists(self))
    {
      if (!state_handler_alloc_state(self))
        {
          if (error)
            {
              *error = g_error_new(1, 3, "Can't allocate new state");
            }
          return FALSE;
        }
    }
  new_state = state_handler_get_state(self);
  new_state->header.version = name_value_container_get_int(container, "version");
  new_state->header.big_endian = name_value_container_get_boolean(container, "big_endian");
  new_state->g_rcptid = name_value_container_get_int(container, "rcptid");

  if ((new_state->header.big_endian && G_BYTE_ORDER == G_LITTLE_ENDIAN) ||
      (!new_state->header.big_endian && G_BYTE_ORDER == G_BIG_ENDIAN))
    {
      new_state->g_rcptid = GUINT32_SWAP_LE_BE(new_state->g_rcptid);
    }
  state_handler_put_state(self);
  return TRUE;
}

static NameValueContainer *
state_handler_rcptid_dump_state(StateHandler *self)
{
  NameValueContainer *result = name_value_container_new();
  RcptidState *state = (RcptidState *) state_handler_get_state(self);
  if (state)
    {
      name_value_container_add_int(result, "version", state->header.version);
      name_value_container_add_boolean(result, "big_endian",
                                       state->header.big_endian);
      name_value_container_add_int64(result, "rcptid", state->g_rcptid);
    }
  state_handler_put_state(self);
  return result;
}

StateHandler *
create_state_handler_rcptid(PersistState *persist_state, const gchar *name)
{
  StateHandler *self = g_new0(StateHandler, 1);
  state_handler_init(self, persist_state, name);
  self->dump_state = state_handler_rcptid_dump_state;
  self->alloc_state = state_handler_rcptid_alloc_state;
  self->load_state = state_handler_rcptid_load_state;
  return self;
}

gchar *
data_to_hex_string(guint8 *data, guint32 length)
{
  gchar *string = NULL;
  gchar *pstr = NULL;
  guint8 *tdata = data;
  guint32 i = 0;
  static const gchar hex_char[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

  if (!length)
    return NULL;

  string = (gchar *)g_malloc0(sizeof(gchar)*length*3 + 1);

  pstr = string;

  *pstr = hex_char[*tdata >> 4];
  pstr++;
  *pstr = hex_char[*tdata & 0x0F];

  for (i = 1; i < length; i++)
    {
      pstr++;
      *pstr = ' ';
      pstr++;
      *pstr = hex_char[tdata[i] >> 4];
      pstr++;
      *pstr = hex_char[tdata[i] & 0x0F];
    }
  pstr++;
  *pstr = '\0';
  return string;
}

gint
_hex_char_to_value(gchar ch)
{
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'A' && ch <= 'F')
    return ch - 'A' + 10;
  if (ch >= 'a' && ch <= 'f')
    return ch - 'a' + 10;
  return -1;
}

guint8 *
hex_string_to_data(const gchar *string, guint32 *size, GError **error)
{
  guint8 *data = NULL;
  guint8 *pdata = NULL;
  const gchar *tstring = string;

  guint32 string_length = strlen(string);
  if (string_length % 3 != 2)
    {
      if (error)
        {
          *error = g_error_new(1, 2, "The length of hex string is not valid.");
        }
      *size = 0;
      return NULL;
    }

  guint32 length = (string_length + 1 ) / 3;
  *size = length;

  data = g_malloc0(sizeof(guint8) * length);

  pdata = data;

  for (guint32 i = 0; i < length; i++)
    {

      gint ch1 = _hex_char_to_value(*tstring);
      if (ch1 < 0)
        goto error;
      tstring++;

      gint ch2 = _hex_char_to_value(*tstring);
      if (ch2 < 0)
        goto error;
      tstring++;

      if (i < length - 1 && *tstring != ' ')
        goto error;
      tstring++;

      *pdata = (guint8)(ch1 * 16 + ch2);
      pdata += 1;
    }
  return data;

error:
  if (data)
    g_free(data);
  *size = 0;
  if (error)
    *error = g_error_new(1, 2, "Error parsing hex string at position %d (character: '%c')", (gint)(tstring - string),
                         *tstring);
  return NULL;
}

static NameValueContainer *
default_state_handler_dump_state(StateHandler *self)
{
  NameValueContainer *result = name_value_container_new();
  state_handler_get_state(self);
  if (self->state)
    {
      gchar *value = data_to_hex_string((gpointer) self->state, (guint32) self->size);
      name_value_container_add_string(result, "value", value);
      g_free(value);
    }
  state_handler_put_state(self);
  return result;
}

gboolean
default_state_handler_load_state(StateHandler *self, NameValueContainer *container, GError **error)
{
  const gchar *string_data = name_value_container_get_value(container, "value");
  if (!string_data)
    {
      if (error)
        {
          *error = g_error_new(1, 0, "Can't find 'value' in the input state");
        }
      return FALSE;
    }
  guint32 size;
  guint8 *data;
  data = hex_string_to_data(string_data, &size, error);
  if (!data)
    return FALSE;

  PersistEntryHandle handle = persist_state_alloc_entry(self->persist_state, self->name, size);
  guint8 *persist_data = persist_state_map_entry(self->persist_state, handle);
  memcpy(persist_data, data, size);
  persist_state_unmap_entry(self->persist_state, handle);
  g_free(data);

  return TRUE;
}

StateHandler *
create_default_state_handler(PersistState *persist_state, const gchar *name)
{
  StateHandler *self = g_new0(StateHandler, 1);
  state_handler_init(self, persist_state, name);
  self->dump_state = default_state_handler_dump_state;
  self->load_state = default_state_handler_load_state;
  return self;
}

STATE_HANDLER_CONSTRUCTOR
state_handler_get_constructor_by_prefix(const gchar *prefix)
{
  STATE_HANDLER_CONSTRUCTOR result = NULL;
  if (persist_state_storage)
    {
      result = g_hash_table_lookup(persist_state_storage, prefix);
    }
  if (!result)
    {
      result = create_default_state_handler;
    }
  return result;
}

void
state_handler_register_constructor(const gchar *prefix,
                                   STATE_HANDLER_CONSTRUCTOR handler)
{
  if (!persist_state_storage)
    {
      persist_state_storage = g_hash_table_new(g_str_hash, g_str_equal);
    }
  g_hash_table_insert(persist_state_storage, (gpointer) prefix, handler);
}

void
state_handler_register_default_constructors()
{
  state_handler_register_constructor(RPCTID_PREFIX,
                                     create_state_handler_rcptid);
}

GList *
persist_state_get_key_list(PersistState *self)
{
  return g_hash_table_get_keys(self->keys);
}

static gboolean
persist_state_commit_store(PersistState *self)
{
  gboolean result;
  gint rename_result;
  rename_result = rename(self->temp_filename, self->committed_filename);
  result = rename_result >= 0;
  return result;
}


/*
 * This function commits the current store as the "persistent" file by
 * renaming the temp file we created to build the loaded
 * information. Once this function returns, then the current
 * persistent file will be visible to the next relaunch of syslog-ng,
 * even if we crashed.
 */
gboolean
persist_state_commit(PersistState *self)
{
//TODO: PersitsState->mode is missing from new version?
//  g_assert(self->mode != persist_mode_dump);
  if (!persist_state_commit_store(self))
    {
      fprintf(stderr, "Can't rename files. source = (%s) destination = (%s) error = (%d)\n",
              self->temp_filename,
              self->committed_filename,
              errno);
      return FALSE;
    }
  return TRUE;
}
