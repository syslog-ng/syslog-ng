#ifndef FILTERX_PRIMITIVE_H_INCLUDED
#define FILTERX_PRIMITIVE_H_INCLUDED

#include "filterx/filterx-object.h"

FILTERX_DECLARE_TYPE(integer);
FILTERX_DECLARE_TYPE(double);
FILTERX_DECLARE_TYPE(boolean);

FilterXObject *filterx_integer_new(gint64 value);
FilterXObject *filterx_double_new(gdouble value);
FilterXObject *filterx_boolean_new(gboolean value);

#endif
