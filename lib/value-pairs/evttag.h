#ifndef VALUE_PAIRS_EVTTAG_H_INCLUDED
#define VALUE_PAIRS_EVTTAG_H_INCLUDED 1

#include "value-pairs.h"
#include "messages.h"

EVTTAG *evt_tag_value_pairs(const char* key, ValuePairs *vp, LogMessage *msg, gint32 seq_num, gint time_zone_mode, LogTemplateOptions *template_options);

#endif
