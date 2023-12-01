#ifndef FILTERX_SCOPE_H_INCLUDED
#define FILTERX_SCOPE_H_INCLUDED

#include "filterx-object.h"
#include "logmsg/logmsg.h"

typedef struct _FilterXScope FilterXScope;

void filterx_scope_sync_to_message(FilterXScope *self, LogMessage *msg);
FilterXObject *filterx_scope_lookup_message_ref(FilterXScope *self, NVHandle handle);
void filterx_scope_register_message_ref(FilterXScope *self, NVHandle handle, FilterXObject *value);
void filterx_scope_store_weak_ref(FilterXScope *self, FilterXObject *object);

FilterXScope *filterx_scope_new(void);
void filterx_scope_free(FilterXScope *self);

#endif