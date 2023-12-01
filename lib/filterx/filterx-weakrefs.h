#ifndef FILTERX_WEAKREFS_H_INCLUDED
#define FILTERX_WEAKREFS_H_INCLUDED

#include "filterx/filterx-object.h"

typedef struct _FilterXWeakRef
{
  FilterXObject *object;
} FilterXWeakRef;

void filterx_weakref_set(FilterXWeakRef *self, FilterXObject *object);
void filterx_weakref_clear(FilterXWeakRef *self);
FilterXObject *filterx_weakref_get(FilterXWeakRef *self);

#endif
