#ifndef CONFIG_PARSE_LIB_H_INCLUDED
#define CONFIG_PARSE_LIB_H_INCLUDED 1

#include "testutils.h"

gboolean parse_config(const gchar *config_to_parse, gint context, gpointer arg, gpointer *result);

#endif
