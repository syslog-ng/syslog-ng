#include "filterx/object-json.h"
#include "filterx/object-null.h"
#include "filterx/object-primitive.h"
#include "filterx/object-list.h"
#include "filterx/object-string.h"
#include "filterx/filterx-weakrefs.h"

typedef struct _FilterXJSON FilterXJSON;
struct _FilterXJSON
{
  FilterXObject super;
  FilterXWeakRef root_container;
  struct json_object *object;
};


static gboolean
_truthy(FilterXObject *s)
{
  return TRUE;
}

static gboolean
_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXJSON *self = (FilterXJSON *) s;

  const gchar *json_repr = json_object_to_json_string_ext(self->object, JSON_C_TO_STRING_PLAIN);
  g_string_assign(repr, json_repr);
  *t = LM_VT_JSON;
  return TRUE;
}

static int
_deep_copy_filterx_object_ref(json_object *src, json_object *parent, const char *key, size_t index, json_object **dst)
{
  int result = json_c_shallow_copy_default(src, parent, key, index, dst);

  if (*dst != NULL)
    {
      /* we need to copy the userdata for primitive types */

      switch (json_object_get_type(src))
        {
        case json_type_null:
        case json_type_boolean:
        case json_type_double:
        case json_type_int:
        case json_type_string:
          FilterXObject *fobj = json_object_get_userdata(src);
          if (fobj)
            filterx_json_associate_cached_object(*dst, fobj);
          break;
        default:
          break;
        }
      return 2;
    }
  return result;
}

static FilterXObject *
_clone(FilterXObject *s)
{
  FilterXJSON *self = (FilterXJSON *) s;

  struct json_object *clone = NULL;
  if (json_object_deep_copy(self->object, &clone, _deep_copy_filterx_object_ref) == 0)
    return filterx_json_new(clone);
  return NULL;
}

static FilterXObject *
_convert_json_to_object(FilterXJSON *self, struct json_object *object)
{
  switch (json_object_get_type(object))
    {
    case json_type_double:
      return filterx_double_new(json_object_get_double(object));
    case json_type_boolean:
      return filterx_boolean_new(json_object_get_boolean(object));
    case json_type_int:
      return filterx_integer_new(json_object_get_int64(object));
    case json_type_string:
      return filterx_string_new(json_object_get_string(object), -1);
    case json_type_object:
    case json_type_array:
    default:
      return filterx_json_new_sub(json_object_get(object), filterx_weakref_get(&self->root_container) ? : filterx_object_ref(&self->super));
    }
}

static FilterXObject *
_convert_json_to_object_cached(FilterXJSON *self, struct json_object *object)
{
  FilterXObject *fobj;

  if (!object || json_object_get_type(object) == json_type_null)
    return filterx_null_new();

  if (json_object_get_type(object) == json_type_double)
    {
      /* this is a workaround to ditch builtin serializer for double objects
       * that are set when parsing from a string representation. 
       * json_object_double_new_ds() will set userdata to the string
       * representation of the number, as extracted from the JSON source.
       * 
       * Changing the value of the double to the same value, ditches this,
       * but only if necessary.
       */
      
      json_object_set_double(object, json_object_get_double(object));
    }

  fobj = json_object_get_userdata(object);

  if (fobj)
    return filterx_object_ref(fobj);

  fobj = _convert_json_to_object(self, object);
  filterx_json_associate_cached_object(object, fobj);
  return fobj;
}

static FilterXObject *
_getattr(FilterXObject *s, const gchar *attr_name)
{
  FilterXJSON *self = (FilterXJSON *) s;
  struct json_object *attr_value;

  if (!json_object_object_get_ex(self->object, attr_name, &attr_value))
    return FALSE;

  return _convert_json_to_object_cached(self, attr_value);
}

static gboolean
_setattr(FilterXObject *s, const gchar *attr_name, FilterXObject *new_value)
{
  FilterXJSON *self = (FilterXJSON *) s;
  struct json_object *attr_value = NULL;

  if (!filterx_object_map_to_json(new_value, &attr_value))
    return FALSE;

  filterx_json_associate_cached_object(attr_value, new_value);

  if (json_object_object_add(self->object, attr_name, attr_value) != 0)
    {
      json_object_put(attr_value);
      return FALSE;
    }
  self->super.modified_in_place = TRUE;
  FilterXObject *root_container = filterx_weakref_get(&self->root_container);
  if (root_container)
    {
      root_container->modified_in_place = TRUE;
      filterx_object_unref(root_container);
    }
  return TRUE;
}

static gboolean
_map_to_json(FilterXObject *s, struct json_object **object)
{
  FilterXJSON *self = (FilterXJSON *) s;

  *object = json_object_get(self->object);
  return TRUE;
}

/* NOTE: consumes root ref */
FilterXObject *
filterx_json_new_sub(struct json_object *object, FilterXObject *root)
{
  FilterXJSON *self = g_new0(FilterXJSON, 1);

  filterx_object_init_instance(&self->super, &FILTERX_TYPE_NAME(json));
  filterx_weakref_set(&self->root_container, root);
  filterx_object_unref(root);
  self->object = object;
  return &self->super;
}

FilterXObject *
filterx_json_new(struct json_object *object)
{
  return filterx_json_new_sub(object, NULL);
}

static void
_free(FilterXObject *s)
{
  FilterXJSON *self = (FilterXJSON *) s;

  json_object_put(self->object);
  filterx_weakref_clear(&self->root_container);
}

FilterXObject *
construct_filterx_json_from_repr(const gchar *repr, gssize repr_len)
{
  struct json_tokener *tokener = json_tokener_new();
  struct json_object *object;

  object = json_tokener_parse_ex(tokener, repr, repr_len < 0 ? strlen(repr) : repr_len);
  if (repr_len >= 0 && json_tokener_get_error(tokener) == json_tokener_continue)
    {
      /* pass the closing NUL character */
      object = json_tokener_parse_ex(tokener, "", 1);
    }

  json_tokener_free(tokener);
  return filterx_json_new(object);
}

static void
_free_cached_filterx_object(struct json_object *object, void *userdata)
{
  FilterXObject *fobj = (FilterXObject *) userdata;
  filterx_object_unref(fobj);
}

void
filterx_json_associate_cached_object(struct json_object *jso, FilterXObject *object)
{
  json_object_set_userdata(jso, filterx_object_ref(object), _free_cached_filterx_object);
}

FILTERX_DEFINE_TYPE(json, FILTERX_TYPE_NAME(object),
  .mutable = TRUE,
  .free_fn = _free,
  .truthy = _truthy,
  .marshal = _marshal,
  .clone = _clone,
  .getattr = _getattr,
  .setattr = _setattr,
  .map_to_json = _map_to_json,
);
