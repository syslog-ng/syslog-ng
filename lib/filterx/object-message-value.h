#ifndef FILTERX_MESSAGE_VALUE_H_INCLUDED
#define FILTERX_MESSAGE_VALUE_H_INCLUDED

#include "filterx/object-message-base.h"

FILTERX_DECLARE_TYPE(message_value);

FilterXObject *filterx_message_value_new_borrowed(const gchar *repr, gssize repr_len, LogMessageValueType type);
FilterXObject *filterx_message_value_new_ref(gchar *repr, gssize repr_len, LogMessageValueType type);
FilterXObject *filterx_message_value_new(const gchar *repr, gssize repr_len, LogMessageValueType type);

#endif
