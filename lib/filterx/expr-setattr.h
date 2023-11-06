#ifndef FILTERX_SETATTR_H_INCLUDED
#define FILTERX_SETATTR_H_INCLUDED

#include "filterx/filterx-expr.h"

FilterXExpr *filterx_setattr_new(FilterXExpr *object, const gchar *attr_name, FilterXExpr *new_value);


#endif
