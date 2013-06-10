#include "state.h"
#include "misc.h"
#include "logmsg.h"
#include "json/json.h"

static GHashTable *persist_state_storage = NULL;

struct _NameValueContainer
{
  struct json_object *container;
};

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

static NameValueContainer*
state_handler_rcptid_dump_state(StateHandler *self)
{
  NameValueContainer *result = name_value_container_new();
  RcptidState *state = (RcptidState *) state_handler_get_state(self);
  if (state)
    {
      name_value_container_add_int(result, "version", state->super.version);
      name_value_container_add_boolean(result, "big_endian",
          state->super.big_endian);
      name_value_container_add_int64(result, "rcptid", state->g_rcptid);
    }
  state_handler_put_state(self);
  return result;
}

StateHandler*
create_state_handler_rcptid(PersistState *persist_state, const gchar *name)
{
  StateHandler *self = g_new0(StateHandler, 1);
  state_handler_init(self, persist_state, name);
  self->dump_state = state_handler_rcptid_dump_state;
  return self;
}

static NameValueContainer*
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

StateHandler*
create_default_state_handler(PersistState *persist_state, const gchar *name)
{
  StateHandler *self = g_new0(StateHandler, 1);
  state_handler_init(self, persist_state, name);
  self->dump_state = default_state_handler_dump_state;
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
