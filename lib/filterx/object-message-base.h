#ifndef FILTERX_OBJECT_MESSAGE_BASE_H
#define FILTERX_OBJECT_MESSAGE_BASE_H

#include "filterx-object.h"

FILTERX_DECLARE_TYPE(message_base);

FilterXObject *_unmarshal_repr(const gchar *repr, gssize repr_len, LogMessageValueType t);
gboolean _is_value_type_pair_truthy(const gchar  *repr, gssize repr_len, LogMessageValueType type);

#endif
