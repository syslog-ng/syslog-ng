#include "expr-json.h"
#include "object-json.h"
#include "scratch-buffers.h"
#include <json-c/json.h>

struct _FilterXJSONKeyValue
{
  gchar *key;
  FilterXExpr *value_expr;
};

FilterXJSONKeyValue *
filterx_json_kv_new(const gchar *key, FilterXExpr *value_expr)
{
  FilterXJSONKeyValue *self = g_new0(FilterXJSONKeyValue, 1);
  self->key = g_strdup(key);
  self->value_expr = value_expr;
  return self;
}

void
filterx_json_kv_free(FilterXJSONKeyValue *self)
{
  g_free(self->key);
  filterx_expr_unref(self->value_expr);
  g_free(self);
}

typedef struct _FilterXJSONExpr
{
  FilterXExpr super;
  GList *key_values;
} FilterXJSONExpr;

static gboolean
_eval_key_value(FilterXJSONExpr *self, struct json_object *object, FilterXJSONKeyValue *kv)
{
  FilterXObject *value = filterx_expr_eval_typed(kv->value_expr);
  gboolean success = FALSE;

  if (!value)
    goto fail;

  struct json_object *value_json = NULL;
  if (!filterx_object_map_to_json(value, &value_json))
    goto fail;

  /* don't store the object in the JSON cache in case it is JSON, as that
   * would cause a circular reference */
  if (!filterx_object_is_type(value, &FILTERX_TYPE_NAME(json)))
    filterx_json_associate_cached_object(value_json, value);
  if (json_object_object_add(object, kv->key, value_json) != 0)
    {
      json_object_put(value_json);
      goto fail;
    }
  success = TRUE;
fail:
  filterx_object_unref(value);
  return success;
}

static FilterXObject *
_eval(FilterXExpr *s)
{
  FilterXJSONExpr *self = (FilterXJSONExpr *) s;
  struct json_object *object;

  object = json_object_new_object();
  for (GList *l = self->key_values; l; l = l->next)
    {
      if (!_eval_key_value(self, object, l->data))
        goto fail;
    }
  return filterx_json_new(object);
fail:
  json_object_put(object);
  return NULL;
}

static void
_free(FilterXExpr *s)
{
  FilterXJSONExpr *self = (FilterXJSONExpr *) s;

  g_list_free_full(self->key_values, (GDestroyNotify) filterx_json_kv_free);
}

FilterXExpr *
filterx_json_expr_new(GList *key_values)
{
  FilterXJSONExpr *self = g_new0(FilterXJSONExpr, 1);

  filterx_expr_init_instance(&self->super);
  self->super.eval = _eval;
  self->super.free_fn = _free;
  self->key_values = key_values;
  return &self->super;
}
