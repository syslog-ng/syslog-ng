#ifndef VALUE_PAIRS_CMDLINE_H_INCLUDED
#define VALUE_PAIRS_CMDLINE_H_INCLUDED 1

#include "value-pairs/value-pairs.h"

ValuePairs *value_pairs_new_from_cmdline(GlobalConfig *cfg,
					 gint argc, gchar **argv,
					 GError **error);

#endif
