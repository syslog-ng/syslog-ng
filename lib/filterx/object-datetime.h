#ifndef FILTERX_OBJECT_DATETIME_H_INCLUDED
#define FILTERX_OBJECT_DATETIME_H_INCLUDED

#include "filterx-object.h"

FILTERX_DECLARE_TYPE(datetime);

FilterXObject *filterx_datetime_new(const UnixTime *ut);

#endif
