#ifndef FILTERX_EXPR_JSON_H_INCLUDED
#define FILTERX_EXPR_JSON_H_INCLUDED

#include "filterx/filterx-expr.h"

typedef struct _FilterXJSONKeyValue FilterXJSONKeyValue;

FilterXJSONKeyValue *filterx_json_kv_new(const gchar *, FilterXExpr *value_expr);
void filterx_json_kv_free(FilterXJSONKeyValue *self);

FilterXExpr *filterx_json_expr_new(GList *key_values);


#endif
