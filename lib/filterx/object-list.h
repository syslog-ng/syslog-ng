#ifndef FILTERX_OBJECT_LIST_H_INCLUDED
#define FILTERX_OBJECT_LIST_H_INCLUDED

#include "filterx-object.h"

FILTERX_DECLARE_TYPE(list);

FilterXObject *filterx_list_new(const gchar *repr, gssize repr_len);

#endif
