#ifndef FILTER_THROTTLE_H_INCLUDED
#define FILTER_THROTTLE_H_INCLUDED

#include "filter-expr.h"

FilterExprNode *filter_throttle_new(void);

gboolean filter_throttle_init(FilterExprNode *s, NVHandle key_handle, gint rate);

#endif