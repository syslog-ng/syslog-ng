#ifndef FILTERX_JSON_H_INCLUDED
#define FILTERX_JSON_H_INCLUDED

#include "filterx/filterx-object.h"
#include <json-c/json.h>

FILTERX_DECLARE_TYPE(json);

void filterx_json_associate_cached_object(struct json_object *json, FilterXObject *object);

FilterXObject *filterx_json_new_sub(struct json_object *object, FilterXObject *root);
FilterXObject *filterx_json_new(struct json_object *object);
FilterXObject *construct_filterx_json_from_repr(const gchar *repr, gssize repr_len);

#endif
