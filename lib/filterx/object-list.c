#include "object-list.h"
#include "object-string.h"
#include "scanner/list-scanner/list-scanner.h"
#include "str-repr/encode.h"

#include <json-c/json.h>

typedef struct _FilterXList
{
  FilterXObject super;
  GPtrArray *objects;
} FilterXList;

static gboolean
_truthy(FilterXObject *s)
{
  return TRUE;
}

static gboolean
_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  FilterXList *self = (FilterXList *) s;

  for (gint i = 0; i < self->objects->len; i++)
    {
      FilterXObject *elem = g_ptr_array_index(self->objects, i);

      if (!filterx_object_is_type(elem, &FILTERX_TYPE_NAME(string)))
        return FALSE;

      if (repr->len > 0 && repr->str[repr->len - 1] != ',')
        g_string_append_c(repr, ',');

      gsize value_len = -1;
      const gchar *value = filterx_string_get_value(elem, &value_len);
      str_repr_encode_append(repr, value, value_len, ",");
    }

  *t = LM_VT_LIST;
  return TRUE;
}

static gboolean
_map_to_json(FilterXObject *s, struct json_object **object)
{
  FilterXList *self = (FilterXList *) s;
  struct json_object *array = json_object_new_array_ext(self->objects->len);

  for (gint i = 0; i < self->objects->len; i++)
    {
      FilterXObject *elem = g_ptr_array_index(self->objects, i);

      struct json_object *elem_json;

      if (!filterx_object_map_to_json(elem, &elem_json))
        {
          json_object_put(array);
          return FALSE;
        }
      json_object_array_add(array, elem_json);
    }

  *object = array;
  return TRUE;
}

static void
_convert_list_to_json_list(FilterXList *self, const gchar *repr, gssize repr_len)
{
  ListScanner scanner;

  list_scanner_init(&scanner);
  list_scanner_input_string(&scanner, repr, repr_len);
  while (list_scanner_scan_next(&scanner))
    {
      g_ptr_array_add(self->objects, filterx_string_new(list_scanner_get_current_value(&scanner), -1));
    }
  list_scanner_deinit(&scanner);
}

FilterXObject *
filterx_list_new(const gchar *repr, gssize repr_len)
{
  FilterXList *self = g_new0(FilterXList, 1);

  filterx_object_init_instance(&self->super, &FILTERX_TYPE_NAME(list));
  self->objects = g_ptr_array_new_with_free_func((GDestroyNotify) filterx_object_unref);
  _convert_list_to_json_list(self, repr, repr_len);
  return &self->super;
}

static void
_free(FilterXObject *s)
{
  FilterXList *self = (FilterXList *) s;

  g_ptr_array_free(self->objects, TRUE);
  filterx_object_free_method(s);
}

FILTERX_DEFINE_TYPE(list, FILTERX_TYPE_NAME(object),
  .mutable = TRUE,
  .free_fn = _free,
  .map_to_json = _map_to_json,
  .marshal = _marshal,
  .truthy = _truthy,
);
