#include "object-null.h"

static gboolean
_truthy(FilterXObject *s)
{
  return FALSE;
}

static gboolean
_marshal(FilterXObject *s, GString *repr, LogMessageValueType *t)
{
  g_string_truncate(repr, 0);
  *t = LM_VT_NULL;
  return TRUE;
}

static gboolean
_map_to_json(FilterXObject *s, struct json_object **object)
{
  *object = NULL;
  return TRUE;
}

FilterXObject *
filterx_null_new(void)
{
  FilterXObject *self = filterx_object_new(&FILTERX_TYPE_NAME(null));
  return self;
}

FILTERX_DEFINE_TYPE(null, FILTERX_TYPE_NAME(object),
  .map_to_json = _map_to_json,
  .marshal = _marshal,
  .truthy = _truthy,
);
