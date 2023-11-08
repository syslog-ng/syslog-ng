#ifndef FILTERX_OBJECT_STRING_H_INCLUDED
#define FILTERX_OBJECT_STRING_H_INCLUDED

#include "filterx-object.h"

FILTERX_DECLARE_TYPE(string);
FILTERX_DECLARE_TYPE(bytes);
FILTERX_DECLARE_TYPE(protobuf);

const gchar *filterx_string_get_value(FilterXObject *s, gsize *length);

FilterXObject *filterx_string_new(const gchar *str, gssize str_len);
FilterXObject *filterx_bytes_new(const gchar *str, gssize str_len);
FilterXObject *filterx_protobuf_new(const gchar *str, gssize str_len);

#endif
